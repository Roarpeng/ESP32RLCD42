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

void DesktopStatus(lv_obj_t* page, lv_obj_t** wifi, lv_obj_t** battery,
                   lv_obj_t** pct, lv_obj_t** sensor) {
    *wifi = lv_image_create(page);
    lv_image_set_src(*wifi, &ui_img_wifi_off);
    lv_obj_set_pos(*wifi, 15, 7);
    *battery = lv_image_create(page);
    lv_image_set_src(*battery, &ui_img_battery_full);
    lv_obj_set_pos(*battery, 230, 7);
    *pct = DesktopLabel(page, "85%", &alibaba_puhui_16, 256, 7, 44);
    if (sensor) {
        *sensor = DesktopLabel(page, "26.5°C", &alibaba_puhui_16, 306, 7, 58);
    }
    DesktopLabel(page, "58%", &alibaba_puhui_16, 365, 7, 40);
    DesktopLine(page, 0, 36, 400, 3);
}

// ===== Pencil 设计 1:1 还原：AI Status Card ====
// 布局: bot 图标 | 细分隔线 | AI 状态文字  (白底黑字)
void DesktopAiBar(lv_obj_t* parent, int x, int y, int w,
                  lv_obj_t** ai_status) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, 32);
    lv_obj_set_style_bg_color(bar, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    // Bot icon placeholder (x=8, y=6, 20x20, font_puhui_16_4 "●")
    lv_obj_t* bot = lv_label_create(bar);
    lv_obj_set_pos(bot, 8, 6);
    lv_obj_set_style_text_font(bot, &font_puhui_16_4, 0);
    lv_obj_set_style_text_color(bot, lv_color_black(), 0);
    lv_label_set_text(bot, "●");

    // 分隔线 (x=36, y=6, w=2, h=20, opacity=0.2)
    lv_obj_t* div = lv_obj_create(bar);
    lv_obj_set_pos(div, 36, 6);
    lv_obj_set_size(div, 2, 20);
    lv_obj_set_style_bg_color(div, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(div, (lv_opa_t)(255 * 0.2), 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_set_style_radius(div, 0, 0);
    lv_obj_set_style_pad_all(div, 0, 0);
    lv_obj_remove_flag(div, LV_OBJ_FLAG_SCROLLABLE);

    // AI 状态文字 (x=46, y=6, 黑色, opacity=0.7)
    if (ai_status) {
        *ai_status = lv_label_create(bar);
        lv_obj_set_pos(*ai_status, 46, 6);
        lv_obj_set_width(*ai_status, w - 50);
        lv_obj_set_style_text_font(*ai_status, &font_puhui_16_4, 0);
        lv_obj_set_style_text_color(*ai_status, lv_color_black(), 0);
        lv_obj_set_style_text_opa(*ai_status, (lv_opa_t)(255 * 0.7), 0);
        lv_obj_set_style_text_align(*ai_status, LV_TEXT_ALIGN_LEFT, 0);
        lv_label_set_long_mode(*ai_status, LV_LABEL_LONG_DOT);
        lv_label_set_text(*ai_status, "AI 待命");
    }
}

// ===== Pencil 设计 1:1 还原：双分隔线 (y=32 + y=34) ====
void DesktopHeaderSeps(lv_obj_t* parent) {
    DesktopLine(parent, 0, 32, 400, 1);
    DesktopLine(parent, 0, 34, 400, 1);
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

    // Pencil 新布局：同步更新所有页面的 AI 状态栏标签
    lv_obj_t* ai_labels[] = {
        clock_ai_status_label_, weather_ai_status_label_,
        quote_ai_status_label_, photo_ai_status_label_,
        pomo_ai_status_label_, music_ai_status_label_,
    };
    for (auto* lbl : ai_labels) {
        if (lbl) lv_label_set_text(lbl, content);
    }

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
    // Pencil 新布局：所有页面 AI 状态栏重置
    lv_obj_t* ai_labels[] = {
        clock_ai_status_label_, weather_ai_status_label_,
        quote_ai_status_label_, photo_ai_status_label_,
        pomo_ai_status_label_, music_ai_status_label_,
    };
    for (auto* lbl : ai_labels) {
        if (lbl) lv_label_set_text(lbl, "AI 待命");
    }
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

    // === Pencil WiFi QR page: AI Bar + Status + seps ===
    wifi_qr_page_ = lv_obj_create(lv_screen_active());
    DesktopPageBase(wifi_qr_page_);

    DesktopAiBar(wifi_qr_page_, 0, 0, 220, nullptr);
    lv_obj_t *qr_wifi_i, *qr_bat_i, *qr_pct_l;
    DesktopStatusRight(wifi_qr_page_, 224, 0, &qr_wifi_i, &qr_bat_i, &qr_pct_l, nullptr);
    DesktopHeaderSeps(wifi_qr_page_);

    // Title: y=50, center, font 18px (use 16px CJK), opacity=0.75
    lv_obj_t* title = DesktopLabel(wifi_qr_page_, "æ«ç æå¼éç½é¡µ",
                                   &font_puhui_16_4, 0, 50, 400, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_opa(title, (lv_opa_t)(255 * 0.75), 0);

    // QR code box: x=120, y=80, w=160, h=160, rounded 4, border 2px
    DesktopObj(wifi_qr_page_, 120, 80, 160, 160, lv_color_white(), 2, 4);

    const int canvas_size = 156;
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
        lv_label_set_text(wifi_qr_code_, "äºç»´ç åå­ä¸è¶³");
    }
    lv_obj_set_pos(wifi_qr_code_, 122, 82);

    // Hotspot label: y=255, center, font 12px (use 14px), opacity=0.5
    std::string ssid_text = "ç­ç¹: ";
    ssid_text += safe_ssid;
    lv_obj_t* ssid_label = DesktopLabel(wifi_qr_page_, ssid_text.c_str(), &font_puhui_14_1, 0, 255, 400, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_opa(ssid_label, (lv_opa_t)(255 * 0.5), 0);

    // URL label: y=270, center, font 12px (use 14px), opacity=0.5
    std::string url_text = "éç½é¡µ: ";
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
    DesktopAiBar(quote_page_, 0, 0, 220, &quote_ai_status_label_);
    DesktopStatusRight(quote_page_, 224, 0,
                       &quote_wifi_icon_img_, &quote_battery_icon_img_,
                       &quote_battery_pct_label_, &quote_sensor_label_);
    DesktopHeaderSeps(quote_page_);

    // === Big quote mark: x=30, y=56, font 52px (use 48), opacity=0.2 ===
    lv_obj_t* qmark = DesktopLabel(quote_page_, "\"", &alibaba_puhui_48, 30, 56, 80);
    lv_obj_set_style_text_opa(qmark, (lv_opa_t)(255 * 0.2), 0);

    // === Quote text: x=50, y=86, w=300, font 22px (use 24), opacity=0.8 ===
    quote_text_label_ = DesktopLabel(quote_page_, "Fall seven times,\nstand up eight.",
                                     &alibaba_puhui_24, 50, 86, 300);
    lv_obj_set_style_text_opa(quote_text_label_, (lv_opa_t)(255 * 0.8), 0);
    {
        Settings quote_settings("quote", false);
        std::string cached_quote = quote_settings.GetString("text", "");
        if (!cached_quote.empty() && quote_text_label_) {
            lv_label_set_text(quote_text_label_, cached_quote.c_str());
        }
    }

    // === NEW QUOTE button: x=120, y=210, w=160, h=42, rounded(21), border 2px ===
    lv_obj_t* btn = DesktopObj(quote_page_, 120, 210, 160, 42, lv_color_white(), 2, 21);
    DesktopLabel(btn, "NEW QUOTE", &alibaba_puhui_16, 30, 11, 100, LV_TEXT_ALIGN_CENTER);

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
    DesktopAiBar(photo_page_, 0, 0, 220, &photo_ai_status_label_);
    DesktopStatusRight(photo_page_, 224, 0,
                       &photo_wifi_icon_img_, &photo_battery_icon_img_,
                       &photo_battery_pct_label_, &photo_sensor_label_);
    DesktopHeaderSeps(photo_page_);

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

    DesktopLabel(photo_empty_overlay_, "相册为空", &alibaba_puhui_24, 0, 40, 370, LV_TEXT_ALIGN_CENTER);
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

    // === Navigation: left arrow x=140,y=270; status x=175,y=273; right arrow x=225,y=270 ===
    DesktopLabel(photo_page_, "<", &alibaba_puhui_24, 140, 270, 30, LV_TEXT_ALIGN_CENTER);
    photo_status_label_ = DesktopLabel(photo_page_, "1/1", &alibaba_puhui_16, 175, 273, 50, LV_TEXT_ALIGN_CENTER);
    DesktopLabel(photo_page_, ">", &alibaba_puhui_24, 225, 270, 30, LV_TEXT_ALIGN_CENTER);

    lv_obj_add_flag(photo_page_, LV_OBJ_FLAG_HIDDEN);
}

void CustomLcdDisplay::SetupClockUI() {
    DisplayLockGuard lock(this);
    clock_page_ = lv_obj_create(lv_screen_active());
    DesktopPageBase(clock_page_);

    // === Pencil: AI Bar (0,0,220) + Status (224,0,175) + double seps ===
    DesktopAiBar(clock_page_, 0, 0, 220, &clock_ai_status_label_);
    DesktopStatusRight(clock_page_, 224, 0,
                       &clock_wifi_icon_img_, &clock_battery_icon_img_,
                       &clock_battery_pct_label_, &clock_sensor_label_);
    DesktopHeaderSeps(clock_page_);

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
        // 缓存到 NVS
        Settings quote_settings("quote", true);
        quote_settings.SetString("text", text);
        ESP_LOGI(TAG, "格言已更新: %s", text);
    }
}
