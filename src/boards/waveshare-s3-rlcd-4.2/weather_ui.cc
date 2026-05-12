#include "custom_lcd_display.h"
#include <esp_log.h>

LV_FONT_DECLARE(alibaba_puhui_16);
LV_FONT_DECLARE(alibaba_puhui_24);
LV_FONT_DECLARE(alibaba_puhui_48);
LV_FONT_DECLARE(alibaba_black_64);

LV_IMAGE_DECLARE(ui_img_wifi_off);
LV_IMAGE_DECLARE(ui_img_battery_full);

namespace {
lv_obj_t* Obj(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t bg,
              int border = 0, int radius = 0) {
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, bg, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, border, 0);
    lv_obj_set_style_border_color(obj, lv_color_black(), 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

lv_obj_t* Label(lv_obj_t* parent, const char* text, const lv_font_t* font,
                int x, int y, int w, lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, text);
    return label;
}

void LineRect(lv_obj_t* parent, int x, int y, int w, int h) {
    Obj(parent, x, y, w, h, lv_color_black(), 0, 0);
}

void Circle(lv_obj_t* parent, int x, int y, int size, lv_color_t color = lv_color_black()) {
    Obj(parent, x, y, size, size, color, 0, LV_RADIUS_CIRCLE);
}
}  // namespace

void CustomLcdDisplay::SetupWeatherUI() {
    DisplayLockGuard lock(this);

    lv_obj_t* root = lv_screen_active();
    weather_page_ = lv_obj_create(root);
    lv_obj_set_size(weather_page_, 400, 300);
    lv_obj_set_pos(weather_page_, 0, 0);
    lv_obj_set_style_bg_color(weather_page_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(weather_page_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(weather_page_, 0, 0);
    lv_obj_set_style_pad_all(weather_page_, 0, 0);
    lv_obj_remove_flag(weather_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(weather_page_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* page = weather_page_;

    wifi_icon_img_ = lv_image_create(page);
    lv_image_set_src(wifi_icon_img_, &ui_img_wifi_off);
    lv_obj_set_pos(wifi_icon_img_, 15, 7);
    battery_icon_img_ = lv_image_create(page);
    lv_image_set_src(battery_icon_img_, &ui_img_battery_full);
    lv_obj_set_pos(battery_icon_img_, 215, 7);
    battery_pct_label_ = Label(page, "85%", &alibaba_puhui_16, 242, 7, 45);
    sensor_label_ = Label(page, "26.5°C", &alibaba_puhui_16, 302, 7, 58);
    Label(page, "58%", &alibaba_puhui_16, 363, 7, 38);
    LineRect(page, 0, 45, 400, 3);

    Circle(page, 65, 82, 54);
    LineRect(page, 43, 76, 9, 18);
    LineRect(page, 78, 58, 9, 20);
    LineRect(page, 110, 105, 18, 9);
    LineRect(page, 32, 108, 18, 9);
    Circle(page, 32, 124, 34);
    Circle(page, 58, 111, 42, lv_color_white());
    Circle(page, 56, 128, 45);
    Circle(page, 81, 136, 34);
    Obj(page, 30, 145, 70, 20, lv_color_black(), 0, 10);

    weather_temp_big_label_ = Label(page, "26°C", &alibaba_puhui_48, 118, 100, 175);
    weather_label_ = Label(page, "Cloudy", &alibaba_puhui_24, 25, 190, 150);
    Obj(page, 27, 251, 14, 14, lv_color_white(), 3, LV_RADIUS_CIRCLE);
    LineRect(page, 32, 264, 4, 7);
    Label(page, "Shenzhen, Nanshan", &alibaba_puhui_24, 50, 244, 250);

    Label(page, "Real feel", &alibaba_puhui_16, 302, 67, 94);
    weather_realfeel_label_ = Label(page, "27°C", &alibaba_puhui_16, 302, 94, 70);
    Label(page, "Humidity", &alibaba_puhui_16, 302, 128, 94);
    weather_humidity_label_ = Label(page, "58%", &alibaba_puhui_16, 302, 155, 70);
    Label(page, "Air Quality", &alibaba_puhui_16, 302, 190, 94);
    weather_air_label_ = Label(page, "28", &alibaba_puhui_16, 302, 217, 70);

    Obj(page, 0, 286, 400, 14, lv_color_black(), 0, 0);
    const char* times[] = {"11:00", "12:00", "13:00", "14:00", "15:00", "16:00"};
    for (int i = 0; i < 6; ++i) {
        lv_obj_t* t = Label(page, times[i], &alibaba_puhui_16, 15 + i * 73, 279, 55, LV_TEXT_ALIGN_CENTER);
        lv_obj_set_style_text_color(t, lv_color_white(), 0);
    }
}
