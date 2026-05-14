// 天气页 UI —— Pencil 设计 1:1 还原
//
// 400x300 黑白单色 RLCD
// 布局：
// ┌─────────────────────────────────────────┐
// │ AI Bar (0,0,220)  │  Status (224,0,175) │
// ├─ sep y=32 ─────── ├ ────────────────────┤
// │ ┌──┐              │  体感温度            │
// │ │☀│ 26°C          │  27°C               │
// │ └──┘              │  湿度               │
// │  多云              │  58%                │
// │                    │  空气质量            │
// │ 📍 深圳, 南山区     │  28                 │
// └─────────────────────────────────────────┘

#include "custom_lcd_display.h"
#include <esp_log.h>

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(alibaba_puhui_16);
LV_FONT_DECLARE(alibaba_puhui_24);
LV_FONT_DECLARE(alibaba_puhui_48);

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

// AI Bar (white bg, black text) — matches Pencil shared component
void WxAiBar(lv_obj_t* parent, lv_obj_t** ai_status) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, 220, 32);
    lv_obj_set_style_bg_color(bar, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* bot = lv_label_create(bar);
    lv_obj_set_pos(bot, 8, 6);
    lv_obj_set_style_text_font(bot, &font_puhui_16_4, 0);
    lv_obj_set_style_text_color(bot, lv_color_black(), 0);
    lv_label_set_text(bot, "●");

    lv_obj_t* div = Obj(bar, 36, 6, 2, 20, lv_color_black(), 0, 0);
    lv_obj_set_style_bg_opa(div, (lv_opa_t)(255 * 0.2), 0);

    if (ai_status) {
        *ai_status = lv_label_create(bar);
        lv_obj_set_pos(*ai_status, 46, 6);
        lv_obj_set_width(*ai_status, 170);
        lv_obj_set_style_text_font(*ai_status, &font_puhui_16_4, 0);
        lv_obj_set_style_text_color(*ai_status, lv_color_black(), 0);
        lv_obj_set_style_text_opa(*ai_status, (lv_opa_t)(255 * 0.7), 0);
        lv_label_set_long_mode(*ai_status, LV_LABEL_LONG_DOT);
        lv_label_set_text(*ai_status, "AI 待命");
    }
}

// Status bar right (WiFi, battery, %, temp, humidity)
void WxStatusRight(lv_obj_t* parent,
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

    // === Top bar: AI Bar + Status + header seps ===
    WxAiBar(page, &weather_ai_status_label_);
    WxStatusRight(page, &wifi_icon_img_, &battery_icon_img_,
                  &battery_pct_label_, &sensor_label_);
    LineRect(page, 0, 32, 400, 1);
    LineRect(page, 0, 34, 400, 1);

    // === Weather icon area: x=40, y=72, 54x54 placeholder square ===
    Obj(page, 40, 72, 54, 54, lv_color_black(), 2, 4);

    // === Big temp: x=100, y=88, font 48px ===
    weather_temp_big_label_ = Label(page, "26°C", &alibaba_puhui_48, 100, 88, 175);

    // === Condition: x=20, y=190, font 24px ===
    weather_label_ = Label(page, "多云", &alibaba_puhui_24, 20, 190, 150);

    // === Location pin: x=22, y=250, 14x14 dot ===
    Circle(page, 22, 250, 14);

    // === Location text: x=44, y=242, font 22px (use 24px) ===
    Label(page, "深圳, 南山区", &alibaba_puhui_24, 44, 242, 250);

    // === Right side metrics (x=290) ===
    // 体感温度 label at y=70, font 16px, opacity=0.5
    lv_obj_t* rf_label = Label(page, "体感温度", &font_puhui_16_4, 290, 70, 100);
    lv_obj_set_style_text_opa(rf_label, (lv_opa_t)(255 * 0.5), 0);
    // 27°C value at y=92, font 20px (use 16px)
    weather_realfeel_label_ = Label(page, "27°C", &alibaba_puhui_24, 290, 92, 70);

    // 湿度 at y=124, opacity=0.5
    lv_obj_t* hm_label = Label(page, "湿度", &font_puhui_16_4, 290, 124, 100);
    lv_obj_set_style_text_opa(hm_label, (lv_opa_t)(255 * 0.5), 0);
    // 58% at y=146
    weather_humidity_label_ = Label(page, "58%", &alibaba_puhui_24, 290, 146, 70);

    // 空气质量 at y=178, opacity=0.5
    lv_obj_t* aq_label = Label(page, "空气质量", &font_puhui_16_4, 290, 178, 100);
    lv_obj_set_style_text_opa(aq_label, (lv_opa_t)(255 * 0.5), 0);
    // 28 at y=200
    weather_air_label_ = Label(page, "28", &alibaba_puhui_24, 290, 200, 70);
}
