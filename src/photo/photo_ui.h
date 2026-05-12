#ifndef PHOTO_UI_H
#define PHOTO_UI_H

#include "image_processor.h"
#include <lvgl.h>

class PhotoUICreator {
public:
    static lv_obj_t* CreatePhotoPage(lv_obj_t* parent);

    static void DisplayBitmap(lv_obj_t* image_obj, const Bitmap1Bit* bmp);

    static void RunFadeTransition(lv_obj_t* image_obj,
                                   const Bitmap1Bit* old_bmp,
                                   const Bitmap1Bit* new_bmp,
                                   int duration_ms);

    static void UpdateStatusBar(lv_obj_t* status_label,
                                 const char* filename,
                                 int current, int total);
};

#endif
