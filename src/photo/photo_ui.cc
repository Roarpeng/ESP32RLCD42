#include "photo_ui.h"
#include <esp_log.h>
#include <cstring>
#include <cstdio>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

LV_FONT_DECLARE(font_puhui_16_4);

static const char* TAG = "PhotoUI";

lv_obj_t* PhotoUICreator::CreatePhotoPage(lv_obj_t* parent) {
    lv_obj_t* page = lv_obj_create(parent);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(page, lv_color_black(), 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* img = lv_image_create(page);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_size(img, 400, 280);
    lv_obj_set_user_data(page, img);

    lv_obj_t* status = lv_label_create(page);
    lv_obj_set_style_text_color(status, lv_color_white(), 0);
    lv_obj_set_style_text_font(status, &font_puhui_16_4, 0);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_label_set_text(status, "No photos");

    lv_obj_set_user_data(status, (void*)"status"); // tag for lookup

    return page;
}

void PhotoUICreator::DisplayBitmap(lv_obj_t* image_obj, const Bitmap1Bit* bmp) {
    if (!bmp || !bmp->data || !image_obj) return;

    // Free previous descriptor if one exists
    lv_image_dsc_t* old_dsc = (lv_image_dsc_t*)lv_image_get_src(image_obj);
    if (old_dsc && old_dsc->data != bmp->data) {
        free(old_dsc);
    }

    lv_image_dsc_t* img_dsc = (lv_image_dsc_t*)malloc(sizeof(lv_image_dsc_t));
    if (!img_dsc) return;

    memset(img_dsc, 0, sizeof(*img_dsc)); // ensure clean struct
    img_dsc->header.cf = LV_COLOR_FORMAT_I1;
    img_dsc->header.w = bmp->width;
    img_dsc->header.h = bmp->height;
    img_dsc->header.stride = bmp->row_bytes;
    img_dsc->data_size = bmp->row_bytes * bmp->height;
    img_dsc->data = (const uint8_t*)bmp->data;

    lv_image_set_src(image_obj, img_dsc);
}

void PhotoUICreator::RunFadeTransition(lv_obj_t* image_obj,
                                        const Bitmap1Bit* old_bmp,
                                        const Bitmap1Bit* new_bmp,
                                        int duration_ms) {
    if (!old_bmp || !new_bmp || !image_obj) return;
    if (old_bmp->width != new_bmp->width || old_bmp->height != new_bmp->height) {
        ESP_LOGE(TAG, "Bitmap dimensions mismatch");
        DisplayBitmap(image_obj, new_bmp);
        return;
    }

    int w = old_bmp->width;
    int h = old_bmp->height;
    int row_bytes = old_bmp->row_bytes;
    int total_steps = 16;

    uint8_t* composite = (uint8_t*)malloc(row_bytes * h);
    if (!composite) { DisplayBitmap(image_obj, new_bmp); return; }

    int step_delay = duration_ms / total_steps;

    for (int step = 0; step <= total_steps; step++) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int byte_idx = y * row_bytes + x / 8;
                int bit_pos = 7 - (x & 7);

                int bayer = ImageProcessor::BayerValue(x, y);
                uint8_t bit;
                if (bayer <= step) {
                    bit = (new_bmp->data[byte_idx] >> bit_pos) & 1;
                } else {
                    bit = (old_bmp->data[byte_idx] >> bit_pos) & 1;
                }

                if (bit) composite[byte_idx] |= (1 << bit_pos);
                else     composite[byte_idx] &= ~(1 << bit_pos);
            }
        }

        Bitmap1Bit tmp;
        tmp.data = composite;
        tmp.width = w;
        tmp.height = h;
        tmp.row_bytes = row_bytes;
        DisplayBitmap(image_obj, &tmp);

        lv_refr_now(nullptr);

        if (step < total_steps) {
            vTaskDelay(pdMS_TO_TICKS(step_delay));
        }
    }

    free(composite);
}

void PhotoUICreator::UpdateStatusBar(lv_obj_t* status_label,
                                      const char* filename,
                                      int current, int total) {
    char buf[128];
    if (filename && *filename && total > 0) {
        const char* name = strrchr(filename, '/');
        name = name ? name + 1 : filename;
        snprintf(buf, sizeof(buf), "%s  %d/%d", name, current + 1, total);
    } else {
        snprintf(buf, sizeof(buf), "No photos");
    }
    lv_label_set_text(status_label, buf);
}
