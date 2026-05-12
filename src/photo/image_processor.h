#ifndef IMAGE_PROCESSOR_H
#define IMAGE_PROCESSOR_H

#include <cstdint>
#include <cstdlib>

struct Bitmap1Bit {
    uint8_t* data = nullptr;
    int width = 0;
    int height = 0;
    int row_bytes = 0;

    void Free() {
        if (data) { free(data); data = nullptr; }
        width = height = row_bytes = 0;
    }
};

class ImageProcessor {
public:
    static Bitmap1Bit* DecodeAndDither(const char* filepath, int target_w, int target_h);
    static int BayerValue(int x, int y);
};

#endif
