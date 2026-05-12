#include "custom_lcd_display.h"
#include <esp_log.h>

LV_FONT_DECLARE(alibaba_puhui_16);
LV_FONT_DECLARE(alibaba_puhui_24);
LV_FONT_DECLARE(alibaba_black_64);
LV_FONT_DECLARE(font_puhui_16_4);

LV_IMAGE_DECLARE(ui_img_wifi_off);
LV_IMAGE_DECLARE(ui_img_battery_full);

namespace {
static const lv_point_precise_t kVLine[] = {{84, 160}, {200, 258}, {316, 160}};
static const lv_point_precise_t kLeftChevron[] = {{50, 145}, {40, 155}, {50, 165}};
static const lv_point_precise_t kRightChevron[] = {{350, 145}, {360, 155}, {350, 165}};

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
                int x, int y, int w, lv_text_align_t align = LV_TEXT_ALIGN_CENTER) {
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

void AddLine(lv_obj_t* parent, const lv_point_precise_t* pts, uint32_t count, int width) {
    lv_obj_t* line = lv_line_create(parent);
    lv_obj_set_size(line, 400, 300);
    lv_line_set_points(line, pts, count);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_color(line, lv_color_black(), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
}

void Dashes(lv_obj_t* parent, int x, int y, int w, int h) {
    for (int px = x; px < x + w; px += 14) {
        LineRect(parent, px, y, 8, 2);
        LineRect(parent, px, y + h - 2, 8, 2);
    }
    for (int py = y; py < y + h; py += 14) {
        LineRect(parent, x, py, 2, 8);
        LineRect(parent, x + w - 2, py, 2, 8);
    }
}
}  // namespace

void CustomLcdDisplay::SetupPomodoroUI() {
    DisplayLockGuard lock(this);

    pomodoro_page_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(pomodoro_page_, 400, 300);
    lv_obj_set_pos(pomodoro_page_, 0, 0);
    lv_obj_set_style_bg_color(pomodoro_page_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(pomodoro_page_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pomodoro_page_, 0, 0);
    lv_obj_set_style_pad_all(pomodoro_page_, 0, 0);
    lv_obj_remove_flag(pomodoro_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(pomodoro_page_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* page = pomodoro_page_;
    pomo_wifi_icon_img_ = lv_image_create(page);
    lv_image_set_src(pomo_wifi_icon_img_, &ui_img_wifi_off);
    lv_obj_set_pos(pomo_wifi_icon_img_, 15, 7);
    pomo_battery_icon_img_ = lv_image_create(page);
    lv_image_set_src(pomo_battery_icon_img_, &ui_img_battery_full);
    lv_obj_set_pos(pomo_battery_icon_img_, 215, 7);
    pomo_battery_pct_label_ = Label(page, "85%", &alibaba_puhui_16, 242, 7, 45);
    pomo_sensor_label_ = Label(page, "26.5°C", &alibaba_puhui_16, 302, 7, 58);
    Label(page, "58%", &alibaba_puhui_16, 363, 7, 38);
    LineRect(page, 0, 29, 400, 3);

    Label(page, "FOCUS ON THE NOW, MEET A BETTER SELF", &alibaba_puhui_16, 0, 43, 400);
    Dashes(page, 112, 67, 176, 190);
    AddLine(page, kVLine, 3, 5);
    AddLine(page, kLeftChevron, 3, 5);
    AddLine(page, kRightChevron, 3, 5);

    pomo_countdown_label_ = Label(page, "25:00", &alibaba_black_64, 130, 122, 140);
    lv_obj_t* start = Obj(page, 146, 213, 108, 36, lv_color_white(), 3, 12);
    Label(start, "Start Focus", &alibaba_puhui_16, 4, 7, 100);

    LineRect(page, 0, 258, 400, 3);
    for (int x = 100; x <= 300; x += 100) {
        LineRect(page, x, 258, 3, 42);
    }
    Label(page, "Tasks", &alibaba_puhui_16, 0, 280, 100);
    Label(page, "Noise", &alibaba_puhui_16, 100, 280, 100);
    Label(page, "Forest", &alibaba_puhui_16, 200, 280, 100);
    Label(page, "Stats", &alibaba_puhui_16, 300, 280, 100);
    Obj(page, 47, 271, 16, 12, lv_color_white(), 2, 0);
    LineRect(page, 52, 274, 7, 2);
    LineRect(page, 52, 278, 7, 2);
    LineRect(page, 154, 270, 4, 14);
    LineRect(page, 158, 270, 10, 3);
    Obj(page, 264, 272, 18, 14, lv_color_white(), 2, 9);
    LineRect(page, 273, 268, 2, 18);
    LineRect(page, 366, 278, 4, 9);
    LineRect(page, 374, 270, 4, 17);
    LineRect(page, 382, 274, 4, 13);

    pomo_state_label_ = nullptr;
    pomo_progress_bar_ = nullptr;
    pomo_info_label_ = nullptr;
    pomo_chat_status_label_ = nullptr;
    pomo_emotion_label_ = nullptr;
    pomo_emotion_img_ = nullptr;
}
