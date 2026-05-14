// CustomLcdDisplay 核心类
//
// 负责：
// - 构造/析构（初始化 RLCD 驱动 + LVGL + 创建 UI）
// - LVGL flush 回调（RGB565 → 1-bit 转换）
// - AI 消息适配（重写小智的 SetChatMessage / SetEmotion / ClearChatMessages）
// - 备忘录功能（加载/刷新备忘录列表）
// - 基类方法重写（UpdateStatusBar / SetTheme）
//
// 其他功能拆分到独立文件：
//   rlcd_driver.cc        - RLCD 硬件驱动
//   weather_ui.cc          - 天气站 UI 布局
//   music_ui.cc            - 音乐页 UI 布局
//   data_update_task.cc    - 后台数据更新任务

#include <vector>
#include <string>
#include <cstring>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include "photo/image_processor.h"
#include "custom_lcd_display.h"
#include "lcd_display.h"
#include "esp_lvgl_port.h"
#include "settings.h"
#include "config.h"
#include "board.h"
#include "application.h"
#include "lvgl_theme.h"
#include "photo_ui.h"
#include "photo/photo_manager.h"
#include <wifi_manager.h>

static const char *TAG = "CustomDisplay";

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(alibaba_puhui_16);
LV_FONT_DECLARE(alibaba_puhui_24);
LV_FONT_DECLARE(alibaba_puhui_48);
LV_FONT_DECLARE(alibaba_black_64);
LV_IMAGE_DECLARE(ui_img_wifi_off);
LV_IMAGE_DECLARE(ui_img_battery_full);

extern "C" {
typedef const uint8_t *qrcode_wrapper_handle_t;
typedef struct {
    void (*display_func)(qrcode_wrapper_handle_t qrcode, void *user_data);
    int max_qrcode_version;
    int qrcode_ecc_level;
    void *user_data;
} qrcode_wrapper_config_t;

enum {
    QRCODE_WRAPPER_ECC_LOW,
    QRCODE_WRAPPER_ECC_MED,
    QRCODE_WRAPPER_ECC_QUART,
    QRCODE_WRAPPER_ECC_HIGH
};

esp_err_t qrcode_wrapper_generate(qrcode_wrapper_config_t *cfg, const char *text);
int qrcode_wrapper_get_size(qrcode_wrapper_handle_t qrcode);
bool qrcode_wrapper_get_module(qrcode_wrapper_handle_t qrcode, int x, int y);
}

namespace {
constexpr int kWifiQrCanvasSize = 170;
constexpr int kWifiQrQuietZoneModules = 4;

struct WifiQrDrawContext {
    lv_obj_t* canvas = nullptr;
    int size = kWifiQrCanvasSize;
};

void DrawFilledRect(lv_obj_t* canvas, int x, int y, int w, int h, lv_color_t color) {
    for (int py = y; py < y + h; ++py) {
        for (int px = x; px < x + w; ++px) {
            lv_canvas_set_px(canvas, px, py, color, LV_OPA_COVER);
        }
    }
}

void DrawQrToCanvas(qrcode_wrapper_handle_t qrcode, void* user_data) {
    auto* ctx = static_cast<WifiQrDrawContext*>(user_data);
    if (!ctx || !ctx->canvas || !qrcode) {
        return;
    }

    const int qr_size = qrcode_wrapper_get_size(qrcode);
    const int total_modules = qr_size + kWifiQrQuietZoneModules * 2;
    int scale = ctx->size / total_modules;
    if (scale < 1) {
        scale = 1;
    }

    const int drawn_size = total_modules * scale;
    const int offset = (ctx->size - drawn_size) / 2;
    lv_canvas_fill_bg(ctx->canvas, lv_color_white(), LV_OPA_COVER);

    for (int y = 0; y < qr_size; ++y) {
        for (int x = 0; x < qr_size; ++x) {
            if (!qrcode_wrapper_get_module(qrcode, x, y)) {
                continue;
            }
            const int px = offset + (x + kWifiQrQuietZoneModules) * scale;
            const int py = offset + (y + kWifiQrQuietZoneModules) * scale;
            DrawFilledRect(ctx->canvas, px, py, scale, scale, lv_color_black());
        }
    }
}
}  // namespace

namespace {
lv_obj_t* DesktopObj(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t bg,
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

lv_obj_t* DesktopLabel(lv_obj_t* parent, const char* text, const lv_font_t* font,
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

void DesktopLine(lv_obj_t* parent, int x, int y, int w, int h) {
    DesktopObj(parent, x, y, w, h, lv_color_black(), 0, 0);
}

void DesktopCircle(lv_obj_t* parent, int x, int y, int size, lv_color_t bg = lv_color_black()) {
    DesktopObj(parent, x, y, size, size, bg, 0, LV_RADIUS_CIRCLE);
}

void DesktopPageBase(lv_obj_t* page) {
    lv_obj_set_size(page, 400, 300);
    lv_obj_set_pos(page, 0, 0);
    lv_obj_set_style_bg_color(page, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_radius(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);
}

// 旧的 DesktopAiBar / DesktopStatus 已被 CustomLcdDisplay::BuildAiBar /
// DesktopStatusRight 完全取代（P0-1 重构），删除以消除 -Wunused-function。

// ===== Pencil 设计 1:1 还原：分隔线 ====
// 1-bit 单色屏无法渲染半透明像素 (Pencil 中 0.15 / 0.12 opacity 的灰线
// 经阈值化后会变白色不可见)，故统一用实心黑线表达 Pencil 中的“顶部分隔”。
//
// Pencil 设计中：
//   - Clock / Weather: 仅一条分隔 (clockHeaderSep / wHeaderSep, y≈32~33)
//   - Quote / Photo / Pomodoro / Music / WiFi QR: 两条 (y=32 + y=34)
// 我们用 1px 实心线表达单条，2 条 1px 线 (y=32 + y=34) 表达双条 ——
// 视觉上分别得到 1px 与 2px 的“顶部边缘”。
void DesktopHeaderSepDouble(lv_obj_t* parent) {
    DesktopLine(parent, 0, 32, 400, 1);
    DesktopLine(parent, 0, 34, 400, 1);
}

void DesktopHeaderSepSingle(lv_obj_t* parent) {
    DesktopLine(parent, 0, 32, 400, 1);
}

// 兼容旧调用：等价于 Pencil 多页通用样式（双分隔）
void DesktopHeaderSeps(lv_obj_t* parent) {
    DesktopHeaderSepDouble(parent);
}

// ===== Pencil 设计 1:1 还原：Status Bar (右侧) ====
void DesktopStatusRight(lv_obj_t* parent, int x, int y,
                        lv_obj_t** wifi, lv_obj_t** battery,
                        lv_obj_t** pct, lv_obj_t** sensor) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_pos(bar, x, y);
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

    *pct = DesktopLabel(bar, "85%", &alibaba_puhui_16, 44, 6, 28);
    lv_obj_set_style_text_color(*pct, lv_color_black(), 0);

    if (sensor) {
        *sensor = DesktopLabel(bar, "26.5°C", &alibaba_puhui_16, 72, 6, 44);
        lv_obj_set_style_text_color(*sensor, lv_color_black(), 0);
    }

    DesktopLabel(bar, "58%", &alibaba_puhui_16, 120, 6, 24);
}

// ===== Pencil 设计 1:1 还原：7 段数码管 =====
// 在父容器内创建一个 44x80 的数码管位，含 7 段矩形
// segs[0..6] = A, B, C, D, E, F, G
void CreateSevenSegDigit(lv_obj_t* parent, int x, int y,
                         lv_obj_t** container, lv_obj_t* segs[7]) {
    *container = lv_obj_create(parent);
    lv_obj_set_pos(*container, x, y);
    lv_obj_set_size(*container, 44, 80);
    lv_obj_set_style_bg_opa(*container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(*container, 0, 0);
    lv_obj_set_style_pad_all(*container, 0, 0);
    lv_obj_remove_flag(*container, LV_OBJ_FLAG_SCROLLABLE);

    // A — 上横 (x:6, y:0, w:32, h:6)
    segs[0] = DesktopObj(*container, 6, 0, 32, 6, lv_color_black(), 0, 1);
    // B — 右上竖 (x:38, y:6, w:6, h:30)
    segs[1] = DesktopObj(*container, 38, 6, 6, 30, lv_color_black(), 0, 1);
    // C — 右下竖 (x:38, y:42, w:6, h:30)
    segs[2] = DesktopObj(*container, 38, 42, 6, 30, lv_color_black(), 0, 1);
    // D — 下横 (x:6, y:74, w:32, h:6)
    segs[3] = DesktopObj(*container, 6, 74, 32, 6, lv_color_black(), 0, 1);
    // E — 左下竖 (x:0, y:42, w:6, h:30)
    segs[4] = DesktopObj(*container, 0, 42, 6, 30, lv_color_black(), 0, 1);
    // F — 左上竖 (x:0, y:6, w:6, h:30)
    segs[5] = DesktopObj(*container, 0, 6, 6, 30, lv_color_black(), 0, 1);
    // G — 中横 (x:6, y:37, w:32, h:6)
    segs[6] = DesktopObj(*container, 6, 37, 32, 6, lv_color_black(), 0, 1);
}

// 设置某位数码管的值 (0-9)，位模式: A B C D E F G
void SetSevenSegDigit(lv_obj_t* segs[7], int value) {
    if (value < 0 || value > 9) return;
    //           0      1      2      3      4      5      6      7      8      9
    static const uint8_t pat[10] = {
        0b1111110, 0b0110000, 0b1101101, 0b1111001, 0b0110011,
        0b1011011, 0b1011111, 0b1110000, 0b1111111, 0b1111011 };
    uint8_t m = pat[value];
    for (int i = 0; i < 7; i++) {
        bool on = (m >> (6 - i)) & 1;
        if (on) {
            lv_obj_remove_flag(segs[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_opa(segs[i], LV_OPA_COVER, 0);
        } else {
            lv_obj_add_flag(segs[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}
}  // namespace

// ===== P0-1 / P0-3：AI 状态卡构建 + 8 态指示 + 动画 =====
//
// 每张 AI 状态卡布局（220x32）：
//   ┌────────────────────────────────────────┐
//   │ [icon 20x20] | div 2x20 | status text  │
//   │  ●  / ○ / ⊘                            │
//   │  +外环（聆听） +音柱（说话）+角标 (! ↑..) │
//   └────────────────────────────────────────┘
//
// icon 区放在 (x=8, y=6, 20x20) 内，所有 icon 元素以容器内坐标定位。
//
// 状态可视化矩阵：
//   状态           | filled | outline | slash | badge | pulse | bars | 默认文案
//   OFFLINE       |        |   ●    |   ●  |       |       |      | "未联网 · 长按 USER 重新配网"
//   PROVISIONING  |        |   ●    |       |  ...  |       |      | "等待手机连接热点..."
//   CONNECTING    |   ●   |        |       |  ...  |       |      | "正在连接 AI..."
//   ONLINE_IDLE   |   ●   |        |       |       |       |      | "AI 待命"
//   LISTENING     |   ●   |        |       |       |   ●  |      | "聆听中..."
//   SPEAKING      |        |        |       |       |       |  ●  | "说话中..."
//   UPGRADING     |   ●   |        |       |   ↑  |       |      | "升级中..."
//   ERROR         |        |   ●    |       |   !  |       |      | "AI 暂不可用，长按 USER 查看"
void CustomLcdDisplay::BuildAiBar(lv_obj_t* parent, int x, int y, int w,
                                   int bar_index, bool dark) {
    AiBarHandles& h = ai_bars_[bar_index];
    h.dark = dark;

    lv_color_t bg = dark ? lv_color_black() : lv_color_white();
    lv_color_t fg = dark ? lv_color_white() : lv_color_black();

    h.bar = lv_obj_create(parent);
    lv_obj_set_pos(h.bar, x, y);
    lv_obj_set_size(h.bar, w, 32);
    lv_obj_set_style_bg_color(h.bar, bg, 0);
    lv_obj_set_style_bg_opa(h.bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(h.bar, 0, 0);
    lv_obj_set_style_radius(h.bar, 0, 0);
    lv_obj_set_style_pad_all(h.bar, 0, 0);
    lv_obj_remove_flag(h.bar, LV_OBJ_FLAG_SCROLLABLE);

    // === icon 区: 20x20 容器在 (8, 6) ===
    // filled: 14x14 实心圆 (中心 7,7 偏移)
    h.icon_filled = lv_obj_create(h.bar);
    lv_obj_set_pos(h.icon_filled, 8 + 3, 6 + 3);
    lv_obj_set_size(h.icon_filled, 14, 14);
    lv_obj_set_style_radius(h.icon_filled, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(h.icon_filled, fg, 0);
    lv_obj_set_style_bg_opa(h.icon_filled, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(h.icon_filled, 0, 0);
    lv_obj_set_style_pad_all(h.icon_filled, 0, 0);
    lv_obj_remove_flag(h.icon_filled, LV_OBJ_FLAG_SCROLLABLE);

    // outline: 14x14 空心环（border-only）
    h.icon_outline = lv_obj_create(h.bar);
    lv_obj_set_pos(h.icon_outline, 8 + 3, 6 + 3);
    lv_obj_set_size(h.icon_outline, 14, 14);
    lv_obj_set_style_radius(h.icon_outline, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(h.icon_outline, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(h.icon_outline, fg, 0);
    lv_obj_set_style_border_width(h.icon_outline, 2, 0);
    lv_obj_set_style_pad_all(h.icon_outline, 0, 0);
    lv_obj_remove_flag(h.icon_outline, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(h.icon_outline, LV_OBJ_FLAG_HIDDEN);

    // slash: 一道 16x2 的对角线（OFFLINE 加在 outline 上）
    h.icon_slash = lv_obj_create(h.bar);
    lv_obj_set_pos(h.icon_slash, 8 + 2, 6 + 9);
    lv_obj_set_size(h.icon_slash, 16, 2);
    lv_obj_set_style_bg_color(h.icon_slash, fg, 0);
    lv_obj_set_style_bg_opa(h.icon_slash, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(h.icon_slash, 0, 0);
    lv_obj_set_style_radius(h.icon_slash, 1, 0);
    lv_obj_set_style_transform_rotation(h.icon_slash, 450, 0);  // 45°
    lv_obj_set_style_pad_all(h.icon_slash, 0, 0);
    lv_obj_remove_flag(h.icon_slash, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(h.icon_slash, LV_OBJ_FLAG_HIDDEN);

    // badge: 角标小文字 (!, ↑, ...) — 紧贴 icon 右下
    h.badge = lv_label_create(h.bar);
    lv_obj_set_pos(h.badge, 8 + 14, 6 + 8);
    lv_obj_set_style_text_font(h.badge, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(h.badge, fg, 0);
    lv_label_set_text(h.badge, "");
    lv_obj_add_flag(h.badge, LV_OBJ_FLAG_HIDDEN);

    // pulse_ring: 聆听时的扩散外环 (LISTENING)
    h.pulse_ring = lv_obj_create(h.bar);
    lv_obj_set_pos(h.pulse_ring, 8, 6);
    lv_obj_set_size(h.pulse_ring, 20, 20);
    lv_obj_set_style_radius(h.pulse_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(h.pulse_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(h.pulse_ring, fg, 0);
    lv_obj_set_style_border_width(h.pulse_ring, 1, 0);
    lv_obj_set_style_pad_all(h.pulse_ring, 0, 0);
    lv_obj_remove_flag(h.pulse_ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(h.pulse_ring, LV_OBJ_FLAG_HIDDEN);

    // speak_bars: 说话时显示的 3 条音柱（替换 icon 区）
    const int bar_x[3] = {8 + 3, 8 + 9, 8 + 15};
    for (int i = 0; i < 3; i++) {
        h.speak_bars[i] = lv_obj_create(h.bar);
        lv_obj_set_pos(h.speak_bars[i], bar_x[i], 6 + 6);
        lv_obj_set_size(h.speak_bars[i], 4, 12);
        lv_obj_set_style_radius(h.speak_bars[i], 1, 0);
        lv_obj_set_style_bg_color(h.speak_bars[i], fg, 0);
        lv_obj_set_style_bg_opa(h.speak_bars[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(h.speak_bars[i], 0, 0);
        lv_obj_set_style_pad_all(h.speak_bars[i], 0, 0);
        lv_obj_remove_flag(h.speak_bars[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(h.speak_bars[i], LV_OBJ_FLAG_HIDDEN);
    }

    // === 分隔线 ===
    lv_obj_t* div = lv_obj_create(h.bar);
    lv_obj_set_pos(div, 36, 6);
    lv_obj_set_size(div, 2, 20);
    lv_obj_set_style_bg_color(div, fg, 0);
    lv_obj_set_style_bg_opa(div, (lv_opa_t)(255 * 0.4), 0);  // 单色屏 >0.5 才渲染为前景
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_set_style_radius(div, 0, 0);
    lv_obj_set_style_pad_all(div, 0, 0);
    lv_obj_remove_flag(div, LV_OBJ_FLAG_SCROLLABLE);

    // === 状态文字 ===
    h.status_label = lv_label_create(h.bar);
    lv_obj_set_pos(h.status_label, 46, 6);
    lv_obj_set_width(h.status_label, w - 50);
    lv_obj_set_style_text_font(h.status_label, &font_puhui_16_4, 0);
    lv_obj_set_style_text_color(h.status_label, fg, 0);
    lv_obj_set_style_text_align(h.status_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(h.status_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(h.status_label, "AI 待命");
}

void CustomLcdDisplay::ApplyAiBarStatus(AiBarHandles& h, AiBarStatus status) {
    if (!h.bar) return;

    // 默认全部 icon 元素隐藏，再按状态打开需要的部分
    auto hide = [](lv_obj_t* o) {
        if (o) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    };
    auto show = [](lv_obj_t* o) {
        if (o) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    };

    hide(h.icon_filled);
    hide(h.icon_outline);
    hide(h.icon_slash);
    hide(h.badge);
    hide(h.pulse_ring);
    for (int i = 0; i < 3; i++) hide(h.speak_bars[i]);

    const char* badge_text = "";

    switch (status) {
        case AiBarStatus::OFFLINE:
            show(h.icon_outline);
            show(h.icon_slash);
            break;
        case AiBarStatus::PROVISIONING:
            show(h.icon_outline);
            badge_text = "...";
            break;
        case AiBarStatus::CONNECTING:
            show(h.icon_filled);
            badge_text = "...";
            break;
        case AiBarStatus::ONLINE_IDLE:
            show(h.icon_filled);
            break;
        case AiBarStatus::LISTENING:
            show(h.icon_filled);
            show(h.pulse_ring);
            break;
        case AiBarStatus::SPEAKING:
            for (int i = 0; i < 3; i++) show(h.speak_bars[i]);
            break;
        case AiBarStatus::UPGRADING:
            show(h.icon_filled);
            badge_text = "↑";
            break;
        case AiBarStatus::ERROR:
            show(h.icon_outline);
            badge_text = "!";
            break;
    }

    if (h.badge) {
        if (badge_text[0] != '\0') {
            lv_label_set_text(h.badge, badge_text);
            show(h.badge);
        }
    }
}

void CustomLcdDisplay::SetAiBarStatusAll(AiBarStatus status) {
    if (status == current_ai_status_) return;
    current_ai_status_ = status;

    for (int i = 0; i < kAiBarCount; i++) {
        ApplyAiBarStatus(ai_bars_[i], status);
    }

    // 状态对应的默认文案（可被 SetAiBarTextAll 覆盖）
    const char* default_text = "AI 待命";
    switch (status) {
        case AiBarStatus::OFFLINE:      default_text = "未联网 · 长按 USER 看详情"; break;
        case AiBarStatus::PROVISIONING: default_text = "等待手机连接热点..."; break;
        case AiBarStatus::CONNECTING:   default_text = "正在连接 AI..."; break;
        case AiBarStatus::ONLINE_IDLE:  default_text = "AI 待命"; break;
        case AiBarStatus::LISTENING:    default_text = "聆听中..."; break;
        case AiBarStatus::SPEAKING:     default_text = "说话中..."; break;
        case AiBarStatus::UPGRADING:    default_text = "固件升级中..."; break;
        case AiBarStatus::ERROR:        default_text = "AI 暂不可用 · 长按 USER 查看"; break;
    }
    SetAiBarTextAll(default_text);

    // 启停动画
    if (status == AiBarStatus::LISTENING) {
        StartListeningAnim();
    } else {
        StopListeningAnim();
    }
    if (status == AiBarStatus::SPEAKING) {
        StartSpeakingAnim();
    } else {
        StopSpeakingAnim();
    }
}

void CustomLcdDisplay::SetAiBarTextAll(const char* text) {
    if (!text) return;
    current_ai_text_ = text;
    for (int i = 0; i < kAiBarCount; i++) {
        if (ai_bars_[i].status_label) {
            lv_label_set_text(ai_bars_[i].status_label, text);
        }
    }
}

// ===== 聆听脉冲：每 80ms 推进一帧，外环从 20→32 + opacity 1→0 (再循环) =====
void CustomLcdDisplay::PulseAnimTimerCb(lv_timer_t* timer) {
    auto* self = static_cast<CustomLcdDisplay*>(lv_timer_get_user_data(timer));
    static int frame = 0;
    frame = (frame + 1) % 12;  // 12 帧 ~ 0.96s 周期

    // size: 20→32, 居中位移 0→-6
    int size = 20 + frame;
    int offset = (size - 20) / 2;
    // opacity: 255 → 0 线性
    lv_opa_t opa = (lv_opa_t)(255 - frame * 21);

    for (int i = 0; i < kAiBarCount; i++) {
        AiBarHandles& h = self->ai_bars_[i];
        if (!h.pulse_ring || lv_obj_has_flag(h.pulse_ring, LV_OBJ_FLAG_HIDDEN)) continue;
        lv_obj_set_size(h.pulse_ring, size, size);
        lv_obj_set_pos(h.pulse_ring, 8 - offset, 6 - offset);
        // 1-bit 屏 opa>127 才显示为前景；这样早期可见、末期隐入
        lv_obj_set_style_border_opa(h.pulse_ring, opa, 0);
    }
}

void CustomLcdDisplay::StartListeningAnim() {
    if (pulse_anim_timer_) return;
    pulse_anim_timer_ = lv_timer_create(PulseAnimTimerCb, 80, this);
}

void CustomLcdDisplay::StopListeningAnim() {
    if (pulse_anim_timer_) {
        lv_timer_delete(pulse_anim_timer_);
        pulse_anim_timer_ = nullptr;
    }
    // 复位外环到初始状态
    for (int i = 0; i < kAiBarCount; i++) {
        AiBarHandles& h = ai_bars_[i];
        if (h.pulse_ring) {
            lv_obj_set_size(h.pulse_ring, 20, 20);
            lv_obj_set_pos(h.pulse_ring, 8, 6);
        }
    }
}

// ===== 说话音柱：每 150ms 切换 3 条柱的高度 (4/8/12 之间循环偏移) =====
void CustomLcdDisplay::SpeakAnimTimerCb(lv_timer_t* timer) {
    auto* self = static_cast<CustomLcdDisplay*>(lv_timer_get_user_data(timer));
    static int phase = 0;
    phase = (phase + 1) % 6;
    // 6 帧节拍模拟随机起伏
    static const int patterns[6][3] = {
        {4, 12, 6}, {12, 4, 8}, {6, 8, 12},
        {12, 6, 4}, {4, 12, 10}, {8, 4, 12}
    };
    for (int i = 0; i < kAiBarCount; i++) {
        AiBarHandles& h = self->ai_bars_[i];
        for (int b = 0; b < 3; b++) {
            if (!h.speak_bars[b]) continue;
            if (lv_obj_has_flag(h.speak_bars[b], LV_OBJ_FLAG_HIDDEN)) continue;
            int hh = patterns[phase][b];
            lv_obj_set_height(h.speak_bars[b], hh);
            lv_obj_set_y(h.speak_bars[b], 6 + (16 - hh));  // 从底部对齐
        }
    }
}

void CustomLcdDisplay::StartSpeakingAnim() {
    if (speak_anim_timer_) return;
    speak_anim_timer_ = lv_timer_create(SpeakAnimTimerCb, 150, this);
}

void CustomLcdDisplay::StopSpeakingAnim() {
    if (speak_anim_timer_) {
        lv_timer_delete(speak_anim_timer_);
        speak_anim_timer_ = nullptr;
    }
}

// ===== P1-1：6 桌面页码指示器（顶栏与正文之间，y=33 一行 6 个 4x4 小点）=====
void CustomLcdDisplay::BuildPageDots(lv_obj_t* parent, int page_index) {
    if (page_index >= kAiBarCount) return;
    bool dark = ai_bars_[page_index].dark;
    lv_color_t fg = dark ? lv_color_white() : lv_color_black();

    // 6 点居中：每点 4x4 + 间距 5 = 总宽 6*4+5*5 = 49
    const int total_w = kPageDotCount * 4 + (kPageDotCount - 1) * 5;
    const int start_x = (400 - total_w) / 2;
    const int y = 24;  // 在 AI 状态卡内顶栏底部下方
    // 实际放在状态栏上面会冲突；放在正文区顶部 y=38（双分隔下方 4px）
    // 但 Pencil 设计大多正文从 y=42 开始；放 y=36 居中在双分隔之间会被覆盖
    // 折中方案：放在正文区域的右上角小尺寸不会与 Pencil 视觉冲突
    // 实际位置：在 status_bar 区域之下，正文区开始之上 (y=36)
    const int dot_y = 36;

    for (int i = 0; i < kPageDotCount; i++) {
        lv_obj_t* dot = lv_obj_create(parent);
        lv_obj_set_pos(dot, start_x + i * 9, dot_y);
        lv_obj_set_size(dot, 4, 4);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, fg, 0);
        if (i == page_index) {
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);  // 实心 = 当前
        } else {
            lv_obj_set_style_bg_opa(dot, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(dot, 1, 0);
            lv_obj_set_style_border_color(dot, fg, 0);
        }
        lv_obj_set_style_pad_all(dot, 0, 0);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        page_dots_[page_index][i] = dot;
    }
    (void)y;
}

void CustomLcdDisplay::RefreshPageDots() {
    int active = static_cast<int>(display_mode_);
    for (int p = 0; p < kAiBarCount; p++) {
        for (int i = 0; i < kPageDotCount; i++) {
            lv_obj_t* dot = page_dots_[p][i];
            if (!dot) continue;
            if (i == active) {
                lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(dot, 0, 0);
            } else {
                lv_obj_set_style_bg_opa(dot, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(dot, 1, 0);
            }
        }
    }
}

// ===== P3-1：省电模式月牙图标 (放在状态栏湿度数字右侧) =====
void CustomLcdDisplay::BuildPowerSaveIcon(lv_obj_t* parent, int page_index) {
    if (page_index >= kAiBarCount) return;
    bool dark = ai_bars_[page_index].dark;
    lv_color_t fg = dark ? lv_color_white() : lv_color_black();

    // 月牙：用一个填充实心圆 + 一个偏移的背景色圆叠加形成"咬一口"效果
    // 放置在右上角 x=388, y=8 的 10x10 区域
    lv_obj_t* moon = lv_obj_create(parent);
    lv_obj_set_pos(moon, 386, 8);
    lv_obj_set_size(moon, 10, 10);
    lv_obj_set_style_radius(moon, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(moon, fg, 0);
    lv_obj_set_style_bg_opa(moon, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(moon, 0, 0);
    lv_obj_set_style_pad_all(moon, 0, 0);
    lv_obj_remove_flag(moon, LV_OBJ_FLAG_SCROLLABLE);

    // 咬口：用页面背景色的小圆覆盖右半
    lv_obj_t* bite = lv_obj_create(moon);
    lv_obj_set_pos(bite, 3, 0);
    lv_obj_set_size(bite, 10, 10);
    lv_obj_set_style_radius(bite, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bite, dark ? lv_color_black() : lv_color_white(), 0);
    lv_obj_set_style_bg_opa(bite, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bite, 0, 0);
    lv_obj_set_style_pad_all(bite, 0, 0);
    lv_obj_remove_flag(bite, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_flag(moon, LV_OBJ_FLAG_HIDDEN);  // 默认隐藏
    power_save_icon_[page_index] = moon;
}

void CustomLcdDisplay::RefreshPowerSaveIcon() {
    bool show = power_saving_;
    if (show == last_power_save_drawn_) return;
    last_power_save_drawn_ = show;
    for (int i = 0; i < kAiBarCount; i++) {
        if (!power_save_icon_[i]) continue;
        if (show) lv_obj_remove_flag(power_save_icon_[i], LV_OBJ_FLAG_HIDDEN);
        else      lv_obj_add_flag(power_save_icon_[i], LV_OBJ_FLAG_HIDDEN);
    }
}


// ===== LVGL flush 回调 =====

void CustomLcdDisplay::Lvgl_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * color_p)
{
    assert(disp != NULL);
    CustomLcdDisplay *self = (CustomLcdDisplay *)lv_display_get_user_data(disp);
    RlcdDriver *rlcd = self->rlcd_;
    uint16_t *buffer = (uint16_t *)color_p;
    for(int y = area->y1; y <= area->y2; y++)
    {
        for(int x = area->x1; x <= area->x2; x++) 
        {
            uint8_t color = (*buffer < 0x7fff) ? ColorBlack : ColorWhite;
            rlcd->RLCD_SetPixel(x, y, color);
            buffer++;
        }
    }
    rlcd->RLCD_Display();
    lv_disp_flush_ready(disp);
}

// ===== 构造 / 析构 =====

CustomLcdDisplay::CustomLcdDisplay(esp_lcd_panel_io_handle_t panel_io,
    esp_lcd_panel_handle_t panel,
    int width, int height, int offset_x, int offset_y,
    bool mirror_x, bool mirror_y, bool swap_xy,
    spi_display_config_t spiconfig,
    spi_host_device_t spi_host) : LcdDisplay(panel_io, panel, width, height)
{
    // 1. 初始化 RLCD 硬件驱动
    rlcd_ = new RlcdDriver(spiconfig, width, height, spi_host);

    // 2. 初始化 LVGL
    ESP_LOGI(TAG, "初始化 LVGL");
    lv_init();
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 2;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);
    lvgl_port_lock(0);

    int transfer = width * height;
    display_ = lv_display_create(width, height);
    lv_display_set_flush_cb(display_, Lvgl_flush_cb);
    lv_display_set_user_data(display_, this);
    size_t lvgl_buffer_size = LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565) * transfer;
    uint8_t *lvgl_buffer1 = (uint8_t *)heap_caps_malloc(lvgl_buffer_size, MALLOC_CAP_SPIRAM);
    assert(lvgl_buffer1);
    lv_display_set_buffers(display_, lvgl_buffer1, NULL, lvgl_buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // 3. 初始化 RLCD 屏幕
    ESP_LOGI(TAG, "初始化 RLCD 屏幕");
    rlcd_->RLCD_Init();

    lvgl_port_unlock();
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "显示初始化失败");
        return;
    }

    // 4. 创建 6 个桌面 UI
    ESP_LOGI(TAG, "创建格言/相册/天气/专注/时钟/音乐桌面 UI");
    SetupQuoteUI();
    SetupPhotoDesktopUI();
    SetupWeatherUI();
    SetupPomodoroUI();
    SetupClockUI();
    SetupMusicUI();
    // 告诉显示框架：当前自定义 UI 已经初始化完成
    // 否则基类的 SetStatus/ShowNotification 会一直误判为“UI 未准备好”
    setup_ui_called_ = true;
    ApplyDisplayMode();

    // 5. 启动时从 NVS 加载上次保存的备忘录
    LoadMemoFromNvs();
}

CustomLcdDisplay::~CustomLcdDisplay() {
    HideWifiProvisioningQr();
    if (update_task_handle_) {
        vTaskDelete(update_task_handle_);
    }
    if (current_photo_bmp_) {
        if (current_photo_bmp_->data) free(current_photo_bmp_->data);
        delete current_photo_bmp_;
    }
    delete rlcd_;
}

// ===== 备忘录功能 =====

void CustomLcdDisplay::LoadMemoFromNvs() {
    // 直接调用 RefreshMemoDisplay 从 NVS 读取并更新 UI
    RefreshMemoDisplay();
}

// 内部版本：不获取锁（调用者必须已持有 DisplayLock）
void CustomLcdDisplay::RefreshMemoDisplayInternal() {
    if (!memo_list_label_) return;

    // 从 NVS 读取 JSON 数组
    Settings settings("memo", false);
    std::string json_str = settings.GetString("items", "");

    if (json_str.empty()) {
        lv_label_set_text(memo_list_label_, "暂无待办");
        return;
    }

    cJSON *arr = cJSON_Parse(json_str.c_str());
    if (!arr || !cJSON_IsArray(arr)) {
        lv_label_set_text(memo_list_label_, "暂无待办");
        if (arr) cJSON_Delete(arr);
        return;
    }

    // 格式化每条备忘为一行: "时间 内容"
    // 卡片高度约 90px，16px 字体每行约 18px，最多显示约 5 行
    std::string display_text;
    int count = cJSON_GetArraySize(arr);
    for (int i = 0; i < count && i < 5; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        cJSON *t = cJSON_GetObjectItem(item, "t");
        cJSON *c = cJSON_GetObjectItem(item, "c");

        if (i > 0) display_text += "\n";

        // 格式：[时间] 内容  或  · 内容（无时间时）
        if (t && cJSON_IsString(t) && strlen(t->valuestring) > 0) {
            display_text += t->valuestring;
            display_text += " ";
        } else {
            display_text += "· ";
        }
        if (c && cJSON_IsString(c)) {
            display_text += c->valuestring;
        }
    }

    // 如果超过 5 条，提示还有更多
    if (count > 5) {
        display_text += "\n...还有" + std::to_string(count - 5) + "条";
    }

    cJSON_Delete(arr);
    lv_label_set_text(memo_list_label_, display_text.c_str());
    ESP_LOGI(TAG, "备忘列表已刷新，共 %d 条", count);
}

// 外部版本：自动获取锁（供 MCP 工具等外部调用）
void CustomLcdDisplay::RefreshMemoDisplay() {
    DisplayLockGuard lock(this);
    RefreshMemoDisplayInternal();
}

// ===== AI 消息适配（重写小智的方法，只更新左下角卡片）=====

void CustomLcdDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (!content || strlen(content) == 0) return;

    // P0-1 + P0-3：经过 SetAiBarTextAll 统一覆盖 6 桌面 + WifiQR 页的 AI 状态文字
    SetAiBarTextAll(content);

    // Legacy：旧 AI 对话卡片（如果存在则带滚动动画）
    if (chat_status_label_) {
        lv_anim_delete(chat_status_label_, nullptr);
        SetShowingSystemInfo(false);
        lv_label_set_text(chat_status_label_, content);
        lv_label_set_long_mode(chat_status_label_, LV_LABEL_LONG_WRAP);
        lv_obj_align(chat_status_label_, LV_ALIGN_LEFT_MID, 64 + 20, 0);

        lv_obj_update_layout(chat_status_label_);
        int label_h = lv_obj_get_height(chat_status_label_);
        lv_obj_t *parent = lv_obj_get_parent(chat_status_label_);
        int visible_h = parent ? lv_obj_get_content_height(parent) : 108;

        if (label_h > visible_h) {
            const int text_x = 64 + 20;
            lv_obj_align(chat_status_label_, LV_ALIGN_TOP_LEFT, text_x, 0);
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, chat_status_label_);
            lv_anim_set_values(&a, 0, -(label_h - visible_h));
            lv_anim_set_delay(&a, 1500);
            lv_anim_set_duration(&a, (label_h - visible_h) * 50);
            lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_repeat_delay(&a, 2000);
            lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
                lv_obj_set_y((lv_obj_t *)obj, v);
            });
            lv_anim_start(&a);
        }
    }

    // Legacy：同步到音乐页/番茄钟页旧 AI 卡片（如果存在）
    if (music_chat_status_label_) {
        lv_label_set_long_mode(music_chat_status_label_, LV_LABEL_LONG_WRAP);
        lv_label_set_text(music_chat_status_label_, content);
    }
    if (pomo_chat_status_label_) {
        lv_label_set_long_mode(pomo_chat_status_label_, LV_LABEL_LONG_WRAP);
        lv_label_set_text(pomo_chat_status_label_, content);
    }
}

void CustomLcdDisplay::SetEmotion(const char* emotion) {
    DisplayLockGuard lock(this);
    
    // 1. 更新左侧文字（完整映射小智所有 21 种表情 + 额外状态）
    const char* text = "待命";
    if (strcmp(emotion, "neutral") == 0)         text = "待命";
    else if (strcmp(emotion, "happy") == 0)      text = "开心";
    else if (strcmp(emotion, "laughing") == 0)   text = "大笑";
    else if (strcmp(emotion, "funny") == 0)      text = "搞笑";
    else if (strcmp(emotion, "sad") == 0)        text = "难过";
    else if (strcmp(emotion, "angry") == 0)      text = "生气";
    else if (strcmp(emotion, "crying") == 0)     text = "哭泣";
    else if (strcmp(emotion, "loving") == 0)     text = "喜爱";
    else if (strcmp(emotion, "embarrassed") == 0) text = "害羞";
    else if (strcmp(emotion, "surprised") == 0)  text = "惊讶";
    else if (strcmp(emotion, "shocked") == 0)    text = "震惊";
    else if (strcmp(emotion, "thinking") == 0)   text = "思考";
    else if (strcmp(emotion, "winking") == 0)    text = "眨眼";
    else if (strcmp(emotion, "cool") == 0)       text = "耍酷";
    else if (strcmp(emotion, "relaxed") == 0)    text = "放松";
    else if (strcmp(emotion, "delicious") == 0)  text = "好吃";
    else if (strcmp(emotion, "kissy") == 0)      text = "亲亲";
    else if (strcmp(emotion, "confident") == 0)  text = "自信";
    else if (strcmp(emotion, "sleepy") == 0)     text = "犯困";
    else if (strcmp(emotion, "silly") == 0)      text = "调皮";
    else if (strcmp(emotion, "confused") == 0)   text = "困惑";
    // 额外状态
    else if (strcmp(emotion, "fear") == 0)       text = "害怕";
    else if (strcmp(emotion, "disgusted") == 0)  text = "嫌弃";
    else if (strcmp(emotion, "microchip_ai") == 0) text = "就绪";
    // 未知情绪也显示中文，不显示英文原文
    else                                         text = "待命";
    
    if (emotion_label_) {
        lv_label_set_text(emotion_label_, text);
    }
    if (music_emotion_label_) {
        lv_label_set_text(music_emotion_label_, text);
    }
    if (pomo_emotion_label_) {
        lv_label_set_text(pomo_emotion_label_, text);
    }
    
    // 2. 尝试加载小智自带的 emoji 图片（天气页 + 音乐页 + 番茄钟页同步更新）
    if (current_theme_) {
        auto emoji_collection = static_cast<LvglTheme*>(current_theme_)->emoji_collection();
        auto image = emoji_collection ? emoji_collection->GetEmojiImage(emotion) : nullptr;
        bool has_image = (image && !image->IsGif());
        
        // 天气页 emoji
        if (emotion_img_) {
            if (has_image) {
                lv_image_set_src(emotion_img_, image->image_dsc());
                lv_obj_remove_flag(emotion_img_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(emotion_img_, LV_OBJ_FLAG_HIDDEN);
            }
        }
        // 音乐页 emoji（同步显示相同的表情图片）
        if (music_emotion_img_) {
            if (has_image) {
                lv_image_set_src(music_emotion_img_, image->image_dsc());
                lv_obj_remove_flag(music_emotion_img_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(music_emotion_img_, LV_OBJ_FLAG_HIDDEN);
            }
        }
        // 番茄钟页 emoji
        if (pomo_emotion_img_) {
            if (has_image) {
                lv_image_set_src(pomo_emotion_img_, image->image_dsc());
                lv_obj_remove_flag(pomo_emotion_img_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(pomo_emotion_img_, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

void CustomLcdDisplay::ClearChatMessages() {
    DisplayLockGuard lock(this);
    if (chat_status_label_) lv_label_set_text(chat_status_label_, "");
    if (music_chat_status_label_) lv_label_set_text(music_chat_status_label_, "");
    if (pomo_chat_status_label_) lv_label_set_text(pomo_chat_status_label_, "");
    // 重置为当前状态对应的默认文案
    AiBarStatus s = current_ai_status_;
    current_ai_status_ = AiBarStatus::ERROR;  // 强制下一次设置触发刷新
    SetAiBarStatusAll(s);
}

void CustomLcdDisplay::ShowWifiProvisioningQr(const char* ssid, const char* url) {
    DisplayLockGuard lock(this);

    if (wifi_qr_page_) {
        lv_obj_del(wifi_qr_page_);
        wifi_qr_page_ = nullptr;
        wifi_qr_code_ = nullptr;
    }
    if (wifi_qr_canvas_buf_) {
        heap_caps_free(wifi_qr_canvas_buf_);
        wifi_qr_canvas_buf_ = nullptr;
    }

    const char* safe_ssid = ssid ? ssid : "";
    const char* safe_url = url ? url : "http://192.168.4.1";
    std::string qr_payload = safe_url;

    // === P0-2 / P3-3：3 步引导版 WiFi 配网页 ===
    // 顶部 AI Bar（PROVISIONING 态）
    // 左半 3 步骤时间线，右半 QR + 底部 SSID/URL
    wifi_qr_page_ = lv_obj_create(lv_screen_active());
    DesktopPageBase(wifi_qr_page_);

    BuildAiBar(wifi_qr_page_, 0, 0, 220, /*bar_index=*/6, /*dark=*/false);  // index 6 = WIFI_QR
    lv_obj_t *qr_wifi_i, *qr_bat_i, *qr_pct_l;
    DesktopStatusRight(wifi_qr_page_, 224, 0, &qr_wifi_i, &qr_bat_i, &qr_pct_l, nullptr);
    DesktopHeaderSeps(wifi_qr_page_);

    // === 左半 3 步骤时间线 (x=14, y=46~150) ===
    // 序号小圆 (黑底白字) + 步骤说明
    auto make_step = [&](int y_pos, const char* num, const char* text) {
        lv_obj_t* circle = lv_obj_create(wifi_qr_page_);
        lv_obj_set_pos(circle, 14, y_pos);
        lv_obj_set_size(circle, 22, 22);
        lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(circle, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(circle, 0, 0);
        lv_obj_set_style_pad_all(circle, 0, 0);
        lv_obj_remove_flag(circle, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* num_lbl = lv_label_create(circle);
        lv_obj_set_pos(num_lbl, 7, 2);
        lv_obj_set_style_text_color(num_lbl, lv_color_white(), 0);
        lv_obj_set_style_text_font(num_lbl, &alibaba_puhui_16, 0);
        lv_label_set_text(num_lbl, num);

        lv_obj_t* step_text = DesktopLabel(wifi_qr_page_, text, &font_puhui_14_1,
                                            44, y_pos + 4, 180, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_style_text_opa(step_text, (lv_opa_t)(255 * 0.85), 0);
    };
    make_step(50,  "1", "用手机连接热点");
    make_step(88,  "2", "扫描右侧二维码");
    make_step(126, "3", "选择 WiFi 输入密码");

    // === 右侧 QR 框 (x=240, y=46, 144x144) ===
    DesktopObj(wifi_qr_page_, 240, 46, 144, 144, lv_color_white(), 2, 4);

    const int canvas_size = 140;
    wifi_qr_canvas_buf_ = heap_caps_malloc(canvas_size * canvas_size * sizeof(lv_color_t),
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!wifi_qr_canvas_buf_) {
        wifi_qr_canvas_buf_ = heap_caps_malloc(canvas_size * canvas_size * sizeof(lv_color_t),
                                               MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    if (wifi_qr_canvas_buf_) {
        wifi_qr_code_ = lv_canvas_create(wifi_qr_page_);
        lv_canvas_set_buffer(wifi_qr_code_, wifi_qr_canvas_buf_, canvas_size,
                             canvas_size, LV_COLOR_FORMAT_RGB565);
        lv_canvas_fill_bg(wifi_qr_code_, lv_color_white(), LV_OPA_COVER);

        WifiQrDrawContext draw_ctx = {
            .canvas = wifi_qr_code_,
            .size = canvas_size,
        };
        qrcode_wrapper_config_t qr_cfg = {};
        qr_cfg.display_func = DrawQrToCanvas;
        qr_cfg.max_qrcode_version = 5;
        qr_cfg.qrcode_ecc_level = QRCODE_WRAPPER_ECC_MED;
        qr_cfg.user_data = &draw_ctx;
        esp_err_t qr_ret = qrcode_wrapper_generate(&qr_cfg, qr_payload.c_str());
        if (qr_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to generate WiFi provisioning QR: %s", esp_err_to_name(qr_ret));
        }
    } else {
        ESP_LOGE(TAG, "Failed to allocate WiFi provisioning QR canvas buffer");
        wifi_qr_code_ = lv_label_create(wifi_qr_page_);
        lv_obj_set_style_text_font(wifi_qr_code_, &font_puhui_16_4, 0);
        lv_obj_set_style_text_color(wifi_qr_code_, lv_color_black(), 0);
        lv_label_set_text(wifi_qr_code_, "二维码内存不足");
    }
    lv_obj_set_pos(wifi_qr_code_, 242, 48);

    // Hotspot label: y=255, center, font 12px (use 14px), opacity=0.5
    std::string ssid_text = "热点: ";
    ssid_text += safe_ssid;
    lv_obj_t* ssid_label = DesktopLabel(wifi_qr_page_, ssid_text.c_str(), &font_puhui_14_1, 0, 255, 400, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_opa(ssid_label, (lv_opa_t)(255 * 0.5), 0);

    // URL label: y=270, center, font 12px (use 14px), opacity=0.5
    std::string url_text = "配网页: ";
    url_text += safe_url;
    lv_obj_t* url_label = DesktopLabel(wifi_qr_page_, url_text.c_str(), &font_puhui_14_1, 0, 270, 400, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_opa(url_label, (lv_opa_t)(255 * 0.5), 0);

    lv_obj_move_to_index(wifi_qr_page_, -1);
    lv_obj_invalidate(wifi_qr_page_);
}

void CustomLcdDisplay::HideWifiProvisioningQr() {
    DisplayLockGuard lock(this);
    if (wifi_qr_page_) {
        lv_obj_del(wifi_qr_page_);
        wifi_qr_page_ = nullptr;
        wifi_qr_code_ = nullptr;
    }
    if (wifi_qr_canvas_buf_) {
        heap_caps_free(wifi_qr_canvas_buf_);
        wifi_qr_canvas_buf_ = nullptr;
    }
}

// ===== 重写状态栏更新（禁用基类的 Font Awesome 文字更新）=====

void CustomLcdDisplay::UpdateStatusBar(bool update_all) {
    // 不调用基类实现！
    // 基类会尝试用 lv_label_set_text 更新 network_label_ 和 battery_label_，
    // 但那些是隐藏的占位标签。我们自己的图片图标由 DataUpdateTask 管理。
}

// ===== 重写主题切换 =====

void CustomLcdDisplay::SetTheme(Theme* theme) {
    // RLCD 是 1-bit 单色屏，只有黑白两色，不需要主题切换。
    // 基类的 SetTheme 会操作 container_、content_、top_bar_ 等控件，
    // 我们的天气站 UI 没有创建这些，直接跳过避免崩溃。
    
    // 但需要保存 theme 指针，SetEmotion 需要用它来加载 emoji 图片
    current_theme_ = theme;
    ESP_LOGI(TAG, "RLCD 单色屏，跳过主题切换（已保存 theme 指针）");
}

void CustomLcdDisplay::ApplyDisplayMode() {
    DisplayLockGuard lock(this);

    // 强制隐藏所有页面，然后只显示当前页面
    lv_obj_t* pages[] = {quote_page_, weather_page_, music_page_,
                         pomodoro_page_, photo_page_, clock_page_};
    for (auto* p : pages) {
        if (p) lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t* active = nullptr;
    switch (display_mode_) {
        case MODE_QUOTE:    active = quote_page_;    break;
        case MODE_PHOTO:    active = photo_page_;    break;
        case MODE_WEATHER:  active = weather_page_;  break;
        case MODE_POMODORO: active = pomodoro_page_; break;
        case MODE_CLOCK:    active = clock_page_;    break;
        case MODE_MUSIC:    active = music_page_;    break;
    }

    if (active) {
        lv_obj_remove_flag(active, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(active);
    }
    RefreshPageDots();
    // P3-5：页面切换瞬间播一次过渡动画（只对前后两个不同的页有效）
    if (active) {
        PlayPageTransition(nullptr, active);
    }
}

void CustomLcdDisplay::CycleDisplayMode() {
    DisplayLockGuard lock(this);
    // 六桌面循环：时钟 → 天气 → 格言 → 相册 → 番茄钟 → 音乐
    switch (display_mode_) {
        case MODE_CLOCK:    display_mode_ = MODE_WEATHER; break;
        case MODE_WEATHER:  display_mode_ = MODE_QUOTE; break;
        case MODE_QUOTE:    display_mode_ = MODE_PHOTO; break;
        case MODE_PHOTO:    display_mode_ = MODE_POMODORO; break;
        case MODE_POMODORO: display_mode_ = MODE_MUSIC; break;
        case MODE_MUSIC:    display_mode_ = MODE_CLOCK; break;
    }
    ApplyDisplayMode();
    const char* name = "未知";
    switch (display_mode_) {
        case MODE_CLOCK:    name = "时钟"; break;
        case MODE_WEATHER:  name = "天气"; break;
        case MODE_QUOTE:    name = "格言"; break;
        case MODE_PHOTO:    name = "相册"; break;
        case MODE_POMODORO: name = "番茄钟"; break;
        case MODE_MUSIC:    name = "音乐"; break;
    }
    ESP_LOGI(TAG, "页面切换: %s", name);
}

// P1-1：USER 长按反向翻页
void CustomLcdDisplay::CycleDisplayModeReverse() {
    DisplayLockGuard lock(this);
    switch (display_mode_) {
        case MODE_CLOCK:    display_mode_ = MODE_MUSIC; break;
        case MODE_WEATHER:  display_mode_ = MODE_CLOCK; break;
        case MODE_QUOTE:    display_mode_ = MODE_WEATHER; break;
        case MODE_PHOTO:    display_mode_ = MODE_QUOTE; break;
        case MODE_POMODORO: display_mode_ = MODE_PHOTO; break;
        case MODE_MUSIC:    display_mode_ = MODE_POMODORO; break;
    }
    ApplyDisplayMode();
    ESP_LOGI(TAG, "页面切换（反向）");
}

void CustomLcdDisplay::SetMusicInfo(const char* title, const char* artist) {
    DisplayLockGuard lock(this);
    if (music_title_label_ == nullptr || music_artist_label_ == nullptr) {
        return;
    }
    lv_label_set_text(music_title_label_, (title && strlen(title) > 0) ? title : "未知歌曲");
    lv_label_set_text(music_artist_label_, (artist && strlen(artist) > 0) ? artist : "未知歌手");
}

void CustomLcdDisplay::SetMusicLyric(const char* lyric) {
    DisplayLockGuard lock(this);
    if (music_lyric_label_ == nullptr) {
        return;
    }

    // 歌词格式："上一句\n当前句\n下一句"（由 application.cc 拼接）
    // 如果没有 \n 分隔符，说明是单行文本（如错误提示），直接显示在当前行
    std::string text(lyric ? lyric : "");
    std::string prev_line, curr_line, next_line;

    size_t first_nl = text.find('\n');
    if (first_nl != std::string::npos) {
        prev_line = text.substr(0, first_nl);
        size_t second_nl = text.find('\n', first_nl + 1);
        if (second_nl != std::string::npos) {
            curr_line = text.substr(first_nl + 1, second_nl - first_nl - 1);
            next_line = text.substr(second_nl + 1);
        } else {
            curr_line = text.substr(first_nl + 1);
        }
    } else {
        // 单行文本（错误提示等），只显示在当前行
        curr_line = text;
    }

    // 更新三个 label
    if (music_lyric_prev_label_) {
        lv_label_set_text(music_lyric_prev_label_, prev_line.c_str());
    }
    lv_label_set_text(music_lyric_label_, curr_line.c_str());
    if (music_lyric_next_label_) {
        lv_label_set_text(music_lyric_next_label_, next_line.c_str());
    }
}

void CustomLcdDisplay::SetMusicProgress(uint32_t current_ms, uint32_t total_ms) {
    DisplayLockGuard lock(this);
    if (music_progress_bar_ == nullptr || music_progress_label_ == nullptr) {
        return;
    }

    if (total_ms > 0) {
        // 有总时长（来自歌词）：正常显示进度条和 "当前 / 总时长"
        if (current_ms > total_ms) {
            current_ms = total_ms;
        }
        lv_bar_set_range(music_progress_bar_, 0, static_cast<int32_t>(total_ms));
        lv_bar_set_value(music_progress_bar_, static_cast<int32_t>(current_ms), LV_ANIM_OFF);

        char progress_text[32];
        snprintf(progress_text, sizeof(progress_text), "%02lu:%02lu / %02lu:%02lu",
                 static_cast<unsigned long>(current_ms / 60000),
                 static_cast<unsigned long>((current_ms / 1000) % 60),
                 static_cast<unsigned long>(total_ms / 60000),
                 static_cast<unsigned long>((total_ms / 1000) % 60));
        lv_label_set_text(music_progress_label_, progress_text);
    } else {
        // 无总时长（没有歌词）：进度条不动，只显示已播放时间
        char progress_text[32];
        snprintf(progress_text, sizeof(progress_text), "%02lu:%02lu",
                 static_cast<unsigned long>(current_ms / 60000),
                 static_cast<unsigned long>((current_ms / 1000) % 60));
        lv_label_set_text(music_progress_label_, progress_text);
    }
}

void CustomLcdDisplay::SwitchToMusicPage() {
    DisplayLockGuard lock(this);
    if (display_mode_ != MODE_MUSIC) {
        display_mode_ = MODE_MUSIC;
        ApplyDisplayMode();
        ESP_LOGI(TAG, "切换到音乐页");
    }
}

void CustomLcdDisplay::SwitchToWeatherPage() {
    DisplayLockGuard lock(this);
    if (display_mode_ != MODE_WEATHER) {
        display_mode_ = MODE_WEATHER;
        ApplyDisplayMode();
        ESP_LOGI(TAG, "自动切换到天气页");
    }
}

// ===== 番茄钟页面方法 =====

void CustomLcdDisplay::SwitchToPomodoroPage() {
    DisplayLockGuard lock(this);
    if (display_mode_ != MODE_POMODORO) {
        display_mode_ = MODE_POMODORO;
        ApplyDisplayMode();
        ESP_LOGI(TAG, "自动切换到番茄钟页");
    }
}

void CustomLcdDisplay::UpdatePomodoroDisplay(const char* state_text, const char* countdown_text,
                                              int progress_permille, const char* info_text) {
    DisplayLockGuard lock(this);
    if (pomo_state_label_ && state_text) {
        lv_label_set_text(pomo_state_label_, state_text);
    }
    if (pomo_countdown_label_ && countdown_text) {
        lv_label_set_text(pomo_countdown_label_, countdown_text);
    }
    if (pomo_progress_bar_) {
        lv_bar_set_value(pomo_progress_bar_, progress_permille, LV_ANIM_OFF);
    }
    if (pomo_info_label_ && info_text) {
        lv_label_set_text(pomo_info_label_, info_text);
    }
}

// ===== 相册页面 =====

void CustomLcdDisplay::SwitchToPhotoPage() {
    {
        DisplayLockGuard lock(this);
        if (display_mode_ != MODE_PHOTO) {
            display_mode_ = MODE_PHOTO;
            ApplyDisplayMode();
            ESP_LOGI(TAG, "切换到相册页");
        }
    }
    UpdatePhotoDesktopStatus();
    UpdatePhotoImage();
}

void CustomLcdDisplay::SwitchToQuotePage() {
    DisplayLockGuard lock(this);
    if (display_mode_ != MODE_QUOTE) {
        display_mode_ = MODE_QUOTE;
        ApplyDisplayMode();
        ESP_LOGI(TAG, "切换到格言页");
    }
}

void CustomLcdDisplay::SwitchToClockPage() {
    DisplayLockGuard lock(this);
    if (display_mode_ != MODE_CLOCK) {
        display_mode_ = MODE_CLOCK;
        ApplyDisplayMode();
        ESP_LOGI(TAG, "切换到时钟页");
    }
}

// ===== 省电模式 =====

void CustomLcdDisplay::NotifyUserActivity() {
    last_activity_ms_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (power_saving_) {
        power_saving_ = false;
        ESP_LOGI(TAG, "用户活动检测到，退出省电模式");
    }
}


void CustomLcdDisplay::SetClockDigit(int pos, int value) {
    if (pos < 0 || pos > 3) return;
    SetSevenSegDigit(clock_digit_segs_[pos], value);
}

void CustomLcdDisplay::SetupQuoteUI() {
    DisplayLockGuard lock(this);
    quote_page_ = lv_obj_create(lv_screen_active());
    DesktopPageBase(quote_page_);

    // === Pencil: AI Bar (0,0,220) + Status (224,0,175) + seps ===
    BuildAiBar(quote_page_, 0, 0, 220, /*bar_index=*/MODE_QUOTE, /*dark=*/false);
    quote_ai_status_label_ = ai_bars_[MODE_QUOTE].status_label;
    DesktopStatusRight(quote_page_, 224, 0,
                       &quote_wifi_icon_img_, &quote_battery_icon_img_,
                       &quote_battery_pct_label_, &quote_sensor_label_);
    DesktopHeaderSeps(quote_page_);
    BuildPageDots(quote_page_, MODE_QUOTE);
    BuildPowerSaveIcon(quote_page_, MODE_QUOTE);

    // === Big quote mark: x=30, y=56, font 52px (use 48), opacity=0.2 ===
    lv_obj_t* qmark = DesktopLabel(quote_page_, "\"", &alibaba_puhui_48, 30, 56, 80);
    lv_obj_set_style_text_opa(qmark, (lv_opa_t)(255 * 0.2), 0);

    // === Quote text: x=50, y=86, w=300, font 22px (use 24), opacity=0.8 ===
    // P1-3：未设置时显示空状态引导（与相册"请通过 Web 上传照片"一致），
    // 而非英文样例，避免被误以为是用户内容。
    quote_text_label_ = DesktopLabel(quote_page_, "尚未设置格言\n双击 USER 刷新",
                                     &font_puhui_16_4, 50, 86, 300);
    lv_obj_set_style_text_opa(quote_text_label_, (lv_opa_t)(255 * 0.55), 0);
    {
        Settings quote_settings("quote", false);
        std::string cached_quote = quote_settings.GetString("text", "");
        if (!cached_quote.empty() && quote_text_label_) {
            lv_label_set_text(quote_text_label_, cached_quote.c_str());
            // 有内容则恢复为正常字号 / 不透明
            lv_obj_set_style_text_font(quote_text_label_, &alibaba_puhui_24, 0);
            lv_obj_set_style_text_opa(quote_text_label_, (lv_opa_t)(255 * 0.8), 0);
        }
    }

    // === NEW QUOTE button: x=120, y=210, w=160, h=42, rounded(21), border 2px ===
    // Pencil 设计：圆角胶囊内含 refresh-cw 图标 + 文字 "NEW QUOTE"，icon 24x24
    // 1-bit 单色屏没有 lucide 字体，用基本图元拼一个 16x16 的旋转刷新箭头剪影：
    //   - 上下两段半圆环（用 4 段短线表示）
    //   - 两个三角形箭头尖（左下/右上）
    lv_obj_t* btn = DesktopObj(quote_page_, 120, 210, 160, 42, lv_color_white(), 2, 21);
    // refresh-cw icon at (8, 11) 内, 16x16
    int ix = 16, iy = 13;  // icon 左上角（按钮内坐标）
    // 上半弧 (左到右)
    DesktopObj(btn, ix + 4, iy + 1, 8, 2, lv_color_black(), 0, 1);   // 顶横
    DesktopObj(btn, ix + 1, iy + 4, 2, 4, lv_color_black(), 0, 1);   // 左竖
    DesktopObj(btn, ix + 12, iy + 1, 2, 4, lv_color_black(), 0, 1);  // 右上箭头柄
    DesktopObj(btn, ix + 11, iy + 5, 4, 2, lv_color_black(), 0, 1);  // 右上箭头横
    // 下半弧
    DesktopObj(btn, ix + 4, iy + 13, 8, 2, lv_color_black(), 0, 1);  // 底横
    DesktopObj(btn, ix + 13, iy + 8, 2, 4, lv_color_black(), 0, 1);  // 右竖
    DesktopObj(btn, ix + 2, iy + 11, 2, 4, lv_color_black(), 0, 1);  // 左下箭头柄
    DesktopObj(btn, ix + 1, iy + 9, 4, 2, lv_color_black(), 0, 1);   // 左下箭头横
    // 文字 "NEW QUOTE" 居中（图标后留 4px 间距，剩余宽度居中显示）
    DesktopLabel(btn, "NEW QUOTE", &alibaba_puhui_16, 40, 11, 110, LV_TEXT_ALIGN_CENTER);

    // === Decorative person figure: head x=340,y=226,12x12; body; arms; legs ===
    DesktopCircle(quote_page_, 340, 226, 12);
    DesktopLine(quote_page_, 345, 238, 2, 20);
    DesktopLine(quote_page_, 335, 248, 22, 2);
    DesktopLine(quote_page_, 340, 258, 2, 18);
    DesktopLine(quote_page_, 348, 258, 2, 18);

    lv_obj_add_flag(quote_page_, LV_OBJ_FLAG_HIDDEN);
}

void CustomLcdDisplay::SetupPhotoDesktopUI() {
    DisplayLockGuard lock(this);
    photo_page_ = lv_obj_create(lv_screen_active());
    DesktopPageBase(photo_page_);

    // === Pencil: AI Bar (0,0,220) + Status (224,0,175) + seps ===
    BuildAiBar(photo_page_, 0, 0, 220, /*bar_index=*/MODE_PHOTO, /*dark=*/false);
    photo_ai_status_label_ = ai_bars_[MODE_PHOTO].status_label;
    DesktopStatusRight(photo_page_, 224, 0,
                       &photo_wifi_icon_img_, &photo_battery_icon_img_,
                       &photo_battery_pct_label_, &photo_sensor_label_);
    DesktopHeaderSeps(photo_page_);
    BuildPageDots(photo_page_, MODE_PHOTO);
    BuildPowerSaveIcon(photo_page_, MODE_PHOTO);

    // === Photo frame: x=15, y=42, w=370, h=190, border 2px ===
    DesktopObj(photo_page_, 15, 42, 370, 190, lv_color_white(), 2, 0);

    // === Photo image: inside frame, 366x186 ===
    photo_image_ = lv_image_create(photo_page_);
    lv_obj_set_size(photo_image_, 366, 186);
    lv_obj_set_pos(photo_image_, 17, 44);

    // === Empty overlay: x=15, y=38, w=370, h=220, rounded 4, border 2px ===
    photo_empty_overlay_ = lv_obj_create(photo_page_);
    lv_obj_set_size(photo_empty_overlay_, 370, 220);
    lv_obj_set_pos(photo_empty_overlay_, 15, 38);
    lv_obj_set_style_bg_color(photo_empty_overlay_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(photo_empty_overlay_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(photo_empty_overlay_, 2, 0);
    lv_obj_set_style_border_color(photo_empty_overlay_, lv_color_black(), 0);
    lv_obj_set_style_radius(photo_empty_overlay_, 4, 0);
    lv_obj_set_style_pad_all(photo_empty_overlay_, 0, 0);
    lv_obj_remove_flag(photo_empty_overlay_, LV_OBJ_FLAG_SCROLLABLE);

    // Pencil: peTitle opacity=0.65 (1-bit 单色屏 >0.5 阈值仍渲染为黑色，
    // 此处显式设置以保持设计一致性)
    lv_obj_t* pe_title = DesktopLabel(photo_empty_overlay_, "相册为空", &alibaba_puhui_24,
                                      0, 40, 370, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_opa(pe_title, (lv_opa_t)(255 * 0.65), 0);
    lv_obj_t* hint1 = DesktopLabel(photo_empty_overlay_, "请通过 Web 上传照片",
                                    &font_puhui_14_1, 0, 90, 370, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_opa(hint1, (lv_opa_t)(255 * 0.45), 0);
    photo_upload_url_label_ = DesktopLabel(photo_empty_overlay_, "http://设备IP",
                                           &font_puhui_14_1, 0, 115, 370, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_opa(photo_upload_url_label_, (lv_opa_t)(255 * 0.45), 0);
    lv_obj_t* hint2 = DesktopLabel(photo_empty_overlay_, "支持 JPG/PNG/BMP 格式",
                                    &font_puhui_14_1, 0, 145, 370, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_opa(hint2, (lv_opa_t)(255 * 0.35), 0);

    lv_obj_add_flag(photo_empty_overlay_, LV_OBJ_FLAG_HIDDEN);

    // === Navigation: Pencil 箭头 20px / 状态文字 14px ===
    // 单色屏可用 ASCII 字体: alibaba_puhui_16 (16px)、alibaba_puhui_24 (24px)，
    // 选择更接近 Pencil 尺寸的 16px 用于箭头与状态文字（避免过大）
    // P2-4：箭头需有"可点击感"，用 24px 字号 + 实色不透明；
    // 状态文字保持 14px、不透明 0.55 表达"次要信息"
    lv_obj_t* arr_l = DesktopLabel(photo_page_, "<", &alibaba_puhui_24, 130, 266, 40, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_opa(arr_l, LV_OPA_COVER, 0);
    photo_status_label_ = DesktopLabel(photo_page_, "1 / 1", &font_puhui_14_1, 170, 275, 60, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_opa(photo_status_label_, (lv_opa_t)(255 * 0.55), 0);
    lv_obj_t* arr_r = DesktopLabel(photo_page_, ">", &alibaba_puhui_24, 230, 266, 40, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_opa(arr_r, LV_OPA_COVER, 0);

    lv_obj_add_flag(photo_page_, LV_OBJ_FLAG_HIDDEN);
}

void CustomLcdDisplay::SetupClockUI() {
    DisplayLockGuard lock(this);
    clock_page_ = lv_obj_create(lv_screen_active());
    DesktopPageBase(clock_page_);

    // === Pencil: AI Bar (0,0,220) + Status (224,0,175) + single sep ===
    // 时钟页 Pencil 设计仅一条分隔线 (clockHeaderSep, opacity=0.15)
    BuildAiBar(clock_page_, 0, 0, 220, /*bar_index=*/MODE_CLOCK, /*dark=*/false);
    clock_ai_status_label_ = ai_bars_[MODE_CLOCK].status_label;
    DesktopStatusRight(clock_page_, 224, 0,
                       &clock_wifi_icon_img_, &clock_battery_icon_img_,
                       &clock_battery_pct_label_, &clock_sensor_label_);
    DesktopHeaderSepSingle(clock_page_);
    BuildPageDots(clock_page_, MODE_CLOCK);
    BuildPowerSaveIcon(clock_page_, MODE_CLOCK);

    // === 4x 7-segment digits at y=77 ===
    CreateSevenSegDigit(clock_page_, 58, 77,
                        &clock_digit_[0], clock_digit_segs_[0]);
    SetSevenSegDigit(clock_digit_segs_[0], 1);

    CreateSevenSegDigit(clock_page_, 125, 77,
                        &clock_digit_[1], clock_digit_segs_[1]);
    SetSevenSegDigit(clock_digit_segs_[1], 0);

    // Colon: two 8x8 rounded dots at y=100 and y=132
    clock_colon_dot_top_ = DesktopObj(clock_page_, 192, 100, 8, 8,
                                      lv_color_black(), 0, 4);
    clock_colon_dot_bot_ = DesktopObj(clock_page_, 192, 132, 8, 8,
                                      lv_color_black(), 0, 4);

    CreateSevenSegDigit(clock_page_, 217, 77,
                        &clock_digit_[2], clock_digit_segs_[2]);
    SetSevenSegDigit(clock_digit_segs_[2], 2);

    CreateSevenSegDigit(clock_page_, 284, 77,
                        &clock_digit_[3], clock_digit_segs_[3]);
    SetSevenSegDigit(clock_digit_segs_[3], 8);

    // === Date: y=195, center, font 20px (use CJK 16px for safety) ===
    clock_date_label_ = DesktopLabel(clock_page_, "06 / 12  周三",
                                     &font_puhui_16_4, 0, 195, 400,
                                     LV_TEXT_ALIGN_CENTER);

    // === Info separator: x=10, y=228, w=380 ===
    DesktopLine(clock_page_, 10, 228, 380, 1);

    // === Big temp: x=10, y=238, font 32px (use 48px) ===
    clock_temp_label_ = DesktopLabel(clock_page_, "26.5°C", &alibaba_puhui_48,
                                     10, 238, 120);

    // === Memo text: x=10, y=274, w=380, font 14px, opacity=0.55 ===
    clock_info_label_ = DesktopLabel(clock_page_, "", &font_puhui_14_1,
                                     10, 274, 380);
    lv_obj_set_style_text_opa(clock_info_label_, (lv_opa_t)(255 * 0.55), 0);
    lv_label_set_long_mode(clock_info_label_, LV_LABEL_LONG_WRAP);

    // P0-3：底部"按 BOOT 说话"提示（首次按 BOOT 后自动消失）
    boot_hint_label_ = DesktopLabel(clock_page_, "按 BOOT 说话 · 单按 USER 切换页面",
                                    &font_puhui_14_1, 0, 285, 400, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_opa(boot_hint_label_, (lv_opa_t)(255 * 0.55), 0);

    lv_obj_add_flag(clock_page_, LV_OBJ_FLAG_HIDDEN);
}

// ===== 相册空状态显示 =====

void CustomLcdDisplay::UpdatePhotoDesktopStatus() {
    DisplayLockGuard lock(this);
    auto& pm = PhotoManager::GetInstance();
    int count = pm.GetPhotoCount();

    if (count == 0) {
        // 无图片：显示上传提示覆盖层
        if (photo_empty_overlay_) {
            lv_obj_remove_flag(photo_empty_overlay_, LV_OBJ_FLAG_HIDDEN);
        }
        // 更新上传 URL（优先用局域网 IP，未联网时用热点地址）
        if (photo_upload_url_label_) {
            auto& wifi = WifiManager::GetInstance();
            std::string ip = wifi.GetIpAddress();
            std::string url;
            if (!ip.empty() && ip != "0.0.0.0") {
                url = "http://" + ip;
            } else {
                url = "http://192.168.4.1";
            }
            lv_label_set_text(photo_upload_url_label_, url.c_str());
        }
        if (photo_status_label_) {
            lv_label_set_text(photo_status_label_, "0 / 0");
        }
        ESP_LOGI(TAG, "相册无图片，显示 Web 上传地址");
    } else {
        // 有图片：隐藏上传提示
        if (photo_empty_overlay_) {
            lv_obj_add_flag(photo_empty_overlay_, LV_OBJ_FLAG_HIDDEN);
        }
        if (photo_status_label_) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d / %d", pm.GetCurrentIndex() + 1, count);
            lv_label_set_text(photo_status_label_, buf);
        }
        ESP_LOGI(TAG, "相册有 %d 张图片", count);
    }
}

void CustomLcdDisplay::UpdatePhotoImage() {
    DisplayLockGuard lock(this);
    auto& pm = PhotoManager::GetInstance();
    std::string path = pm.GetCurrentPhotoPath();
    if (path.empty() || !photo_image_) return;

    ESP_LOGI(TAG, "Loading photo: %s", path.c_str());
    
    Bitmap1Bit* bmp = ImageProcessor::DecodeAndDither(path.c_str(), 366, 194);
    if (bmp) {
        PhotoUICreator::DisplayBitmap(photo_image_, bmp);
        
        if (current_photo_bmp_) {
            if (current_photo_bmp_->data) free(current_photo_bmp_->data);
            delete current_photo_bmp_;
        }
        current_photo_bmp_ = bmp;
    } else {
        ESP_LOGE(TAG, "Failed to decode and dither photo");
    }
}

// ===== 格言动态更新 =====

void CustomLcdDisplay::UpdateQuoteText(const char* text) {
    DisplayLockGuard lock(this);
    if (quote_text_label_ && text && strlen(text) > 0) {
        lv_label_set_text(quote_text_label_, text);
        // P1-3：从空状态切回正常显示样式
        lv_obj_set_style_text_font(quote_text_label_, &alibaba_puhui_24, 0);
        lv_obj_set_style_text_opa(quote_text_label_, (lv_opa_t)(255 * 0.8), 0);
        // 缓存到 NVS
        Settings quote_settings("quote", true);
        quote_settings.SetString("text", text);
        ESP_LOGI(TAG, "格言已更新: %s", text);
    }
}

// ===== P0-3：BOOT 提示控制 =====
void CustomLcdDisplay::DismissBootHint() {
    DisplayLockGuard lock(this);
    boot_hint_dismissed_ = true;
    if (boot_hint_label_) {
        lv_obj_add_flag(boot_hint_label_, LV_OBJ_FLAG_HIDDEN);
    }
}

// ===== P3-4：设置 / 关于 模态层 =====
void CustomLcdDisplay::BuildSettingsOverlay() {
    settings_overlay_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(settings_overlay_, 400, 300);
    lv_obj_set_pos(settings_overlay_, 0, 0);
    lv_obj_set_style_bg_color(settings_overlay_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(settings_overlay_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(settings_overlay_, 0, 0);
    lv_obj_set_style_pad_all(settings_overlay_, 0, 0);
    lv_obj_set_style_radius(settings_overlay_, 0, 0);
    lv_obj_remove_flag(settings_overlay_, LV_OBJ_FLAG_SCROLLABLE);

    // 标题（顶部黑底白字"设置 · 关于"反白条 36px）
    lv_obj_t* title_bar = lv_obj_create(settings_overlay_);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_size(title_bar, 400, 36);
    lv_obj_set_style_bg_color(title_bar, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 0, 0);
    lv_obj_remove_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_bar);
    lv_obj_set_pos(title, 0, 8);
    lv_obj_set_width(title, 400);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &font_puhui_16_4, 0);
    lv_label_set_text(title, "设置 · 关于");

    // 信息区
    settings_info_label_ = lv_label_create(settings_overlay_);
    lv_obj_set_pos(settings_info_label_, 16, 48);
    lv_obj_set_width(settings_info_label_, 368);
    lv_obj_set_style_text_color(settings_info_label_, lv_color_black(), 0);
    lv_obj_set_style_text_font(settings_info_label_, &font_puhui_14_1, 0);
    lv_label_set_long_mode(settings_info_label_, LV_LABEL_LONG_WRAP);
    lv_label_set_text(settings_info_label_, "");

    // 底部操作区
    lv_obj_t* hint = lv_label_create(settings_overlay_);
    lv_obj_set_pos(hint, 0, 264);
    lv_obj_set_width(hint, 400);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(hint, lv_color_black(), 0);
    lv_obj_set_style_text_font(hint, &font_puhui_14_1, 0);
    lv_obj_set_style_text_opa(hint, (lv_opa_t)(255 * 0.55), 0);
    lv_label_set_text(hint, "BOOT 长按返回 · USER 单击重新配网");

    lv_obj_add_flag(settings_overlay_, LV_OBJ_FLAG_HIDDEN);
}

void CustomLcdDisplay::RefreshSettingsOverlay() {
    if (!settings_info_label_) return;

    auto& app = Application::GetInstance();
    DeviceState ds = app.GetDeviceState();
    auto& wifi = WifiManager::GetInstance();
    std::string ip = wifi.GetIpAddress();
    if (ip.empty()) ip = "未联网";

    Settings websocket_settings("websocket", false);
    std::string ws_url = websocket_settings.GetString("url", "");
    Settings mqtt_settings("mqtt", false);
    std::string mqtt_ep = mqtt_settings.GetString("endpoint", "");

    const char* protocol_name = "—";
    if (!mqtt_ep.empty()) protocol_name = "MQTT";
    else if (!ws_url.empty()) protocol_name = "WebSocket";

    const char* state_text = "未知";
    switch (ds) {
        case kDeviceStateStarting:        state_text = "启动中"; break;
        case kDeviceStateWifiConfiguring: state_text = "配网中"; break;
        case kDeviceStateActivating:      state_text = "激活中"; break;
        case kDeviceStateUpgrading:       state_text = "升级中"; break;
        case kDeviceStateIdle:            state_text = "在线 · 待命"; break;
        case kDeviceStateConnecting:      state_text = "连接对话信道"; break;
        case kDeviceStateListening:       state_text = "聆听中"; break;
        case kDeviceStateSpeaking:        state_text = "说话中"; break;
        case kDeviceStateFatalError:      state_text = "致命错误"; break;
        default: break;
    }

    Settings ota_settings("wifi", false);
    std::string ota_url = ota_settings.GetString("ota_url", "");

    char buf[640];
    snprintf(buf, sizeof(buf),
        "状态: %s\n"
        "IP: %s\n"
        "AI 后端: %s\n"
        "OTA: %s\n"
        "\n"
        "USER 单击 → 重新配网\n"
        "BOOT 长按 → 返回主页",
        state_text,
        ip.c_str(),
        protocol_name,
        (ota_url.empty() ? "默认 (Xiaozhi)" : ota_url.c_str()));
    lv_label_set_text(settings_info_label_, buf);
}

void CustomLcdDisplay::ToggleSettingsOverlay() {
    DisplayLockGuard lock(this);
    if (!settings_overlay_) BuildSettingsOverlay();
    bool hidden = lv_obj_has_flag(settings_overlay_, LV_OBJ_FLAG_HIDDEN);
    if (hidden) {
        RefreshSettingsOverlay();
        lv_obj_remove_flag(settings_overlay_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(settings_overlay_);
        ESP_LOGI(TAG, "打开设置 / 关于");
    } else {
        lv_obj_add_flag(settings_overlay_, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "关闭设置 / 关于");
    }
}

// ===== P3-5：页面切换过渡动画 =====
// 单色屏不能做透明渐变。用一道 400×300 黑色面板从顶部下落覆盖整页 (~150ms)
// 再向下滑出 (~150ms)，总耗时 ~300ms。视觉上像幕布翻页。
namespace {
struct TransitionCtx {
    lv_obj_t* curtain;
    int phase;  // 0=down 1=up
};

void TransitionAnimCb(void* obj, int32_t v) {
    auto* curtain = static_cast<lv_obj_t*>(obj);
    lv_obj_set_y(curtain, v);
}

void TransitionReady(lv_anim_t* a) {
    auto* ctx = static_cast<TransitionCtx*>(lv_anim_get_user_data(a));
    if (!ctx) return;
    if (ctx->phase == 0) {
        // 下落到位后，反向滑出
        ctx->phase = 1;
        lv_anim_t up;
        lv_anim_init(&up);
        lv_anim_set_var(&up, ctx->curtain);
        lv_anim_set_values(&up, 0, 300);
        lv_anim_set_duration(&up, 150);
        lv_anim_set_exec_cb(&up, TransitionAnimCb);
        lv_anim_set_user_data(&up, ctx);
        lv_anim_set_completed_cb(&up, TransitionReady);
        lv_anim_start(&up);
    } else {
        // 收尾：删除幕布
        if (ctx->curtain) lv_obj_del(ctx->curtain);
        delete ctx;
    }
}
}  // namespace

void CustomLcdDisplay::PlayPageTransition(lv_obj_t* /*prev_page*/, lv_obj_t* /*next_page*/) {
    // 创建一道 400x300 黑色幕布，初始 y = -300 (在屏幕之上)
    lv_obj_t* curtain = lv_obj_create(lv_screen_active());
    lv_obj_set_size(curtain, 400, 300);
    lv_obj_set_pos(curtain, 0, -300);
    lv_obj_set_style_bg_color(curtain, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(curtain, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(curtain, 0, 0);
    lv_obj_set_style_pad_all(curtain, 0, 0);
    lv_obj_set_style_radius(curtain, 0, 0);
    lv_obj_remove_flag(curtain, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(curtain);

    auto* ctx = new TransitionCtx{curtain, 0};
    lv_anim_t down;
    lv_anim_init(&down);
    lv_anim_set_var(&down, curtain);
    lv_anim_set_values(&down, -300, 0);
    lv_anim_set_duration(&down, 150);
    lv_anim_set_exec_cb(&down, TransitionAnimCb);
    lv_anim_set_user_data(&down, ctx);
    lv_anim_set_completed_cb(&down, TransitionReady);
    lv_anim_start(&down);
}
