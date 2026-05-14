#include "image_processor.h"
#include <esp_log.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <esp_heap_caps.h>
#include "display/lvgl_display/jpg/jpeg_to_image.h"

static const char* TAG = "ImageProcessor";

// Bayer 4x4 ordered dither matrix
static const uint8_t bayer_4x4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5}
};

int ImageProcessor::BayerValue(int x, int y) {
    return bayer_4x4[y & 3][x & 3];
}

// Bilinear scale RGB565 image
static uint16_t* BilinearScale(const uint8_t* src_rgb, int src_w, int src_h,
                                int dst_w, int dst_h) {
    uint16_t* dst = (uint16_t*)malloc(dst_w * dst_h * sizeof(uint16_t));
    if (!dst) return nullptr;

    const uint16_t* src = (const uint16_t*)src_rgb;
    float x_ratio = (float)(src_w - 1) / dst_w;
    float y_ratio = (float)(src_h - 1) / dst_h;

    for (int dy = 0; dy < dst_h; dy++) {
        float sy = dy * y_ratio;
        int sy_int = (int)sy;
        float sy_frac = sy - sy_int;
        int sy_next = (sy_int + 1 < src_h) ? sy_int + 1 : sy_int;

        for (int dx = 0; dx < dst_w; dx++) {
            float sx = dx * x_ratio;
            int sx_int = (int)sx;
            float sx_frac = sx - sx_int;
            int sx_next = (sx_int + 1 < src_w) ? sx_int + 1 : sx_int;

            uint16_t p00 = src[sy_int * src_w + sx_int];
            uint16_t p10 = src[sy_int * src_w + sx_next];
            uint16_t p01 = src[sy_next * src_w + sx_int];
            uint16_t p11 = src[sy_next * src_w + sx_next];

            for (int c = 0; c < 3; c++) {
                int shift = (c == 0) ? 11 : ((c == 1) ? 5 : 0);
                int mask = (c == 1) ? 0x3F : 0x1F;
                int v00 = (p00 >> shift) & mask;
                int v10 = (p10 >> shift) & mask;
                int v01 = (p01 >> shift) & mask;
                int v11 = (p11 >> shift) & mask;

                float top = v00 * (1 - sx_frac) + v10 * sx_frac;
                float bot = v01 * (1 - sx_frac) + v11 * sx_frac;
                int val = (int)(top * (1 - sy_frac) + bot * sy_frac + 0.5f);

                dst[dy * dst_w + dx] &= ~(mask << shift);
                dst[dy * dst_w + dx] |= (val << shift);
            }
        }
    }
    return dst;
}

// Floyd-Steinberg error diffusion dithering: RGB565 → 1-bit
static void FloydSteinbergDither(const uint16_t* rgb565, int w, int h, Bitmap1Bit* out) {
    float* lum = (float*)malloc(w * h * sizeof(float));
    if (!lum) return;

    for (int i = 0; i < w * h; i++) {
        uint16_t p = rgb565[i];
        int r = (p >> 11) & 0x1F;
        int g = (p >> 5) & 0x3F;
        int b = p & 0x1F;
        lum[i] = 0.299f * (r * 255.0f / 31.0f)
               + 0.587f * (g * 255.0f / 63.0f)
               + 0.114f * (b * 255.0f / 31.0f);
    }

    out->width = w;
    out->height = h;
    out->row_bytes = (w + 7) / 8;
    out->data = (uint8_t*)calloc(out->row_bytes * h, 1);
    if (!out->data) { free(lum); return; }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float old = lum[y * w + x];
            uint8_t new_val = (old > 127.0f) ? 255 : 0;
            float error = old - new_val;

            if (new_val == 255) {
                int byte_idx = y * out->row_bytes + x / 8;
                out->data[byte_idx] |= (0x80 >> (x & 7));
            }

            if (x + 1 < w) lum[y * w + (x + 1)] += error * (7.0f / 16.0f);
            if (y + 1 < h) {
                if (x > 0) lum[(y + 1) * w + (x - 1)] += error * (3.0f / 16.0f);
                lum[(y + 1) * w + x] += error * (5.0f / 16.0f);
                if (x + 1 < w) lum[(y + 1) * w + (x + 1)] += error * (1.0f / 16.0f);
            }
        }
    }
    free(lum);
}

Bitmap1Bit* ImageProcessor::DecodeAndDither(const char* filepath, int target_w, int target_h) {
    Bitmap1Bit* result = new Bitmap1Bit();
    if (!result) return nullptr;

    // Open file for reading into memory
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        delete result;
        return nullptr;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0 || fsize > 300 * 1024) {
        ESP_LOGE(TAG, "File too large or empty: %ld bytes", fsize);
        fclose(f);
        delete result;
        return nullptr;
    }
    uint8_t* jpeg_data = (uint8_t*)malloc(fsize);
    if (!jpeg_data) { fclose(f); delete result; return nullptr; }
    fread(jpeg_data, 1, fsize, f);
    fclose(f);

    // Decode JPEG header to get dimensions
    // Use a minimal JPEG parser to extract image dimensions
    int img_w = 0, img_h = 0;
    // Scan for SOF0 (0xFFC0) or SOF2 (0xFFC2, progressive) marker to get dimensions
    for (long i = 0; i < fsize - 9; i++) {
        if (jpeg_data[i] == 0xFF && (jpeg_data[i + 1] == 0xC0 || jpeg_data[i + 1] == 0xC2)) {
            img_h = (jpeg_data[i + 5] << 8) | jpeg_data[i + 6];
            img_w = (jpeg_data[i + 7] << 8) | jpeg_data[i + 8];
            break;
        }
    }
    if (img_w == 0 || img_h == 0) {
        ESP_LOGE(TAG, "Failed to parse JPEG dimensions");
        free(jpeg_data);
        delete result;
        return nullptr;
    }

    uint16_t* rgb565 = nullptr;
    uint8_t* out_data = nullptr;
    size_t out_len = 0, dec_w = 0, dec_h = 0, dec_stride = 0;
    
    if (jpeg_to_image(jpeg_data, fsize, &out_data, &out_len, &dec_w, &dec_h, &dec_stride) == ESP_OK && out_data != nullptr) {
        rgb565 = (uint16_t*)out_data;
        img_w = dec_w;
        img_h = dec_h;
    } else {
        ESP_LOGE(TAG, "Failed to decode JPEG image");
    }

    free(jpeg_data);

    if (!rgb565) {
        delete result;
        return nullptr;
    }

    // Scale to target size
    uint16_t* scaled = BilinearScale((const uint8_t*)rgb565, img_w, img_h, target_w, target_h);
    heap_caps_free(rgb565);

    if (!scaled) {
        delete result;
        return nullptr;
    }

    // Dither to 1-bit
    FloydSteinbergDither(scaled, target_w, target_h, result);
    free(scaled);

    if (!result->data) {
        delete result;
        return nullptr;
    }

    ESP_LOGI(TAG, "Decoded %s: %dx%d -> 1-bit %dx%d (%d bytes)",
             filepath, img_w, img_h, target_w, target_h,
             result->row_bytes * result->height);
    return result;
}
