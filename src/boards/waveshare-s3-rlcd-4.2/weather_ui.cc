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

// （旧的 WxAiBar 已迁移到 CustomLcdDisplay::BuildAiBar，统一 8 态可视化 / P0-1）

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

    // === Top bar: AI Bar + Status + single header sep ===
    // Pencil: wHeaderSep 仅一条 (y=33, opacity=0.15) —— 单色屏用 1px 实线还原
    BuildAiBar(page, 0, 0, 220, /*bar_index=*/1, /*dark=*/false);  // MODE_WEATHER=1
    weather_ai_status_label_ = ai_bars_[1].status_label;
    WxStatusRight(page, &wifi_icon_img_, &battery_icon_img_,
                  &battery_pct_label_, &sensor_label_);
    LineRect(page, 0, 32, 400, 1);
    BuildPageDots(page, 1);
    BuildPowerSaveIcon(page, 1);

    // === Cloud-sun weather icon at (40,72), 54x54 ===
    // Pencil 用 lucide "cloud-sun"。1-bit 单色屏用基本几何图元拼出可识别剪影：
    //   太阳：右上 14x14 黑圆 + 4 道短射线
    //   云朵：3 圆叠加 + 平底矩形构成黑色云剪影
    Circle(page, 70, 76, 14);                    // sun disc
    LineRect(page, 76, 72, 2, 4);                // ray top
    LineRect(page, 86, 82, 4, 2);                // ray right
    LineRect(page, 84, 75, 3, 3);                // ray top-right (small square)
    LineRect(page, 76, 92, 2, 4);                // ray bottom
    Circle(page, 40, 92, 22);                    // cloud left puff
    Circle(page, 50, 84, 28);                    // cloud center top puff
    Circle(page, 62, 94, 22);                    // cloud right puff
    Obj(page, 45, 110, 36, 6, lv_color_black(), 0, 0);  // cloud flat bottom

    // === Big temp: x=100, y=88, font 48px ===
    weather_temp_big_label_ = Label(page, "26°C", &alibaba_puhui_48, 100, 88, 175);

    // === Condition: x=20, y=190, font 24px ===
    weather_label_ = Label(page, "多云", &alibaba_puhui_24, 20, 190, 150);

    // === Location pin at (22,250), 14x14 ===
    // Pencil 用 lucide "map-pin"。1-bit 用「上圆 + 下三角尾」的剪影模拟：
    //   头部：8x8 圆 (位于上方)
    //   尾部：2x4 竖向尾梢
    Circle(page, 25, 250, 8);                    // pin head
    Obj(page, 28, 258, 2, 4, lv_color_black(), 0, 0);  // pin tail

    // === Location text: x=44, y=242, font 22px (use 24px) ===
    Label(page, "深圳, 南山区", &alibaba_puhui_24, 44, 242, 250);

    // === Right side metrics (x=290) ===
    // 标题 16px opacity 0.5；数值 20px (无 20px CJK 字体，回退到 16px CJK)
    // 体感温度 label at y=70
    lv_obj_t* rf_label = Label(page, "体感温度", &font_puhui_16_4, 290, 70, 100);
    lv_obj_set_style_text_opa(rf_label, (lv_opa_t)(255 * 0.5), 0);
    // 27°C value at y=92
    weather_realfeel_label_ = Label(page, "27°C", &alibaba_puhui_16, 290, 92, 70);

    // 湿度 at y=124
    lv_obj_t* hm_label = Label(page, "湿度", &font_puhui_16_4, 290, 124, 100);
    lv_obj_set_style_text_opa(hm_label, (lv_opa_t)(255 * 0.5), 0);
    weather_humidity_label_ = Label(page, "58%", &alibaba_puhui_16, 290, 146, 70);

    // 空气质量 at y=178
    lv_obj_t* aq_label = Label(page, "空气质量", &font_puhui_16_4, 290, 178, 100);
    lv_obj_set_style_text_opa(aq_label, (lv_opa_t)(255 * 0.5), 0);
    weather_air_label_ = Label(page, "28", &alibaba_puhui_16, 290, 200, 70);
}
