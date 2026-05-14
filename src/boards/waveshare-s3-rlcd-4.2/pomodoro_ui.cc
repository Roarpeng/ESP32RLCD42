// 番茄钟页 UI —— Pencil 设计 1:1 还原
//
// 400x300 黑白单色 RLCD
// 布局：
// ┌──────────────────────────────────────────┐
// │ AI Bar (0,0,220)  │  Status (224,0,175)  │
// ├─ sep y=32 ──────── ├ ────────────────────┤
// │      FOCUS ON THE NOW (y=52, center)     │
// │  ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐        │
// │  ┊           25:00             ┊        │
// │  ┊       [ Start Focus ]      ┊        │
// │  └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘        │
// │    双击 USER 键开始专注 (y=262, center)   │
// └──────────────────────────────────────────┘

#include "custom_lcd_display.h"
#include <esp_log.h>

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(alibaba_puhui_16);
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

// （旧 PomoAiBar 迁移到 CustomLcdDisplay::BuildAiBar / P0-1）

void PomoStatusRight(lv_obj_t* parent,
                     lv_obj_t** wifi, lv_obj_t** battery,
                     lv_obj_t** pct, lv_obj_t** sensor) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_pos(bar, 224, 0);
    lv_obj_set_size(bar, 175, 30);
    lv_obj_set_style_bg_color(bar, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    *wifi = lv_image_create(bar);
    lv_image_set_src(*wifi, &ui_img_wifi_off);
    lv_obj_set_pos(*wifi, 0, 6);

    *battery = lv_image_create(bar);
    lv_image_set_src(*battery, &ui_img_battery_full);
    lv_obj_set_pos(*battery, 24, 6);

    *pct = Label(bar, "85%", &alibaba_puhui_16, 44, 6, 28);
    if (sensor) {
        *sensor = Label(bar, "26.5°C", &alibaba_puhui_16, 72, 6, 44);
    }
    Label(bar, "58%", &alibaba_puhui_16, 120, 6, 24);
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

    // === Top bar: AI Bar + Status + header seps ===
    BuildAiBar(page, 0, 0, 220, /*bar_index=*/4, /*dark=*/false);  // MODE_POMODORO=4
    pomo_ai_status_label_ = ai_bars_[4].status_label;
    PomoStatusRight(page, &pomo_wifi_icon_img_, &pomo_battery_icon_img_,
                    &pomo_battery_pct_label_, &pomo_sensor_label_);
    LineRect(page, 0, 32, 400, 1);
    LineRect(page, 0, 34, 400, 1);
    BuildPageDots(page, 4);
    BuildPowerSaveIcon(page, 4);

    // === State text: y=52, center, font 18px (use 16px CJK), opacity=0.75 ===
    // P2-3：中文统一（原 "FOCUS ON THE NOW"）
    pomo_state_label_ = Label(page, "专注此刻", &font_puhui_16_4, 0, 52, 400);
    lv_obj_set_style_text_opa(pomo_state_label_, (lv_opa_t)(255 * 0.75), 0);

    // === Box: x=80, y=88, w=240, h=160, rounded 16, border 2px ===
    // Pencil 中是实线圆角矩形 (cornerRadius 16, stroke thickness 2)，非虚线
    Obj(page, 80, 88, 240, 160, lv_color_white(), 2, 16);

    // === Countdown: centered in box, font 60px (use 64px bold) ===
    pomo_countdown_label_ = Label(page, "25:00", &alibaba_black_64, 80, 120, 240);

    // === Start button: centered in box, w=128, h=36, rounded 18, border 3px ===
    // P2-3：中文统一（原 "Start Focus"）
    lv_obj_t* start = Obj(page, 136, 200, 128, 36, lv_color_white(), 3, 18);
    Label(start, "开始专注", &font_puhui_16_4, 0, 8, 128);

    // === Info text: y=262, center, font 14px, opacity=0.45 ===
    pomo_info_label_ = Label(page, "双击 USER 键开始专注", &font_puhui_14_1, 0, 262, 400);
    lv_obj_set_style_text_opa(pomo_info_label_, (lv_opa_t)(255 * 0.45), 0);

    pomo_progress_bar_ = nullptr;
    pomo_chat_status_label_ = nullptr;
    pomo_emotion_label_ = nullptr;
    pomo_emotion_img_ = nullptr;
}
