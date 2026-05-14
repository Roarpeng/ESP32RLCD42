#ifndef __CUSTOM_LCD_DISPLAY_H__
#define __CUSTOM_LCD_DISPLAY_H__

#include <atomic>
#include <string>
#include <driver/gpio.h>
#include "lcd_display.h"
#include "rlcd_driver.h"
#include "managers/sensor_manager.h"
#include "managers/weather_manager.h"
#include "photo/photo_ui.h"

// ===== AI 状态卡的 8 态指示 (P0-1 / P0-3) =====
// 每态对应 AI 顶部状态卡的 icon 形态和默认文案。
// 状态由 DataUpdateTask 周期性根据 DeviceState 推断后调用 SetAiBarStatusAll() 设置。
// SetChatMessage() 会临时覆盖文案，下次状态变化恢复。
enum class AiBarStatus {
    OFFLINE,        // 未联网（无 SSID 或 STA 未连）
    PROVISIONING,   // 配网模式（kDeviceStateWifiConfiguring）
    CONNECTING,     // 联网/激活/对话信道连接中
    ONLINE_IDLE,    // 待命（kDeviceStateIdle）
    LISTENING,      // 聆听用户说话（kDeviceStateListening，icon 带脉冲环）
    SPEAKING,       // 播放 TTS（kDeviceStateSpeaking，icon 替换为 3 条音柱）
    UPGRADING,      // 固件升级（kDeviceStateUpgrading）
    ERROR,          // 错误（kDeviceStateFatalError 或 Alert/Network 错误）
};

// 一张 AI 状态卡的所有可视元素 handle。每个页面创建后填一份；
// SetAiBarStatusAll/SetAiBarTextAll 遍历所有非空 handle 同步更新。
struct AiBarHandles {
    lv_obj_t* bar = nullptr;            // 顶层容器
    lv_obj_t* icon_filled = nullptr;    // 实心圆（ONLINE_IDLE / LISTENING / SPEAKING）
    lv_obj_t* icon_outline = nullptr;   // 空心圆（OFFLINE / PROVISIONING / ERROR）
    lv_obj_t* icon_slash = nullptr;     // 斜杠（OFFLINE）
    lv_obj_t* badge = nullptr;          // "!" / "↑" / "..." 角标
    lv_obj_t* pulse_ring = nullptr;     // 聆听脉冲外环
    lv_obj_t* speak_bars[3] = {nullptr};// 说话音柱
    lv_obj_t* status_label = nullptr;   // 文案
    bool dark = false;                  // 是否深色页（音乐页）
};

// 天气站 + AI 混合显示
// 
// 屏幕布局 (400x300, 1-bit 单色 RLCD)：
// ┌──────────────────┬──────────────────┐
// │   时钟卡片(248x128) │  日历卡片(130x128) │
// │    "14:30"        │   TUE / 15      │
// │                   │   晴 25°C       │
// ├──────────────────┼──────────────────┤
// │   AI 对话(252x122) │  备忘录(126x122)  │
// │  "聆听中..."      │   MEMO          │
// └──────────────────┴──────────────────┘
// 状态栏浮在右上角（WiFi + 电池 + 温湿度）
//
// 代码拆分为多个文件：
//   rlcd_driver.h/cc      - RLCD 硬件驱动层（SPI、像素映射、初始化命令）
//   weather_ui.cc          - 天气站 UI 布局（SetupWeatherUI）
//   music_ui.cc            - 音乐页 UI 布局（SetupMusicUI）
//   pomodoro_ui.cc         - 番茄钟 UI 布局（SetupPomodoroUI）
//   data_update_task.cc    - 后台数据更新任务（时间/天气/传感器/电池/WiFi/AI状态/番茄钟）
//   custom_lcd_display.cc  - 核心类（构造/析构/AI适配/备忘录/基类重写）
class CustomLcdDisplay : public LcdDisplay {
private:
    enum DisplayMode {
        MODE_CLOCK = 0,
        MODE_WEATHER = 1,
        MODE_QUOTE = 2,
        MODE_PHOTO = 3,
        MODE_POMODORO = 4,
        MODE_MUSIC = 5,
    };
    DisplayMode display_mode_ = MODE_CLOCK;

    // RLCD 硬件驱动（独立模块，负责 SPI 通信和像素操作）
    RlcdDriver *rlcd_ = nullptr;
    lv_obj_t *weather_page_ = nullptr;
    lv_obj_t *music_page_ = nullptr;
    lv_obj_t *pomodoro_page_ = nullptr;
    lv_obj_t *quote_page_ = nullptr;
    lv_obj_t *clock_page_ = nullptr;
    lv_obj_t *photo_page_ = nullptr;
    lv_obj_t *photo_image_ = nullptr;
    lv_obj_t *photo_status_label_ = nullptr;
    lv_obj_t *wifi_qr_page_ = nullptr;
    lv_obj_t *wifi_qr_code_ = nullptr;
    void *wifi_qr_canvas_buf_ = nullptr;
    lv_obj_t *photo_empty_overlay_ = nullptr;   // 相册无图时的覆盖层
    lv_obj_t *photo_upload_url_label_ = nullptr; // 上传地址标签
    Bitmap1Bit *current_photo_bmp_ = nullptr;

    // ===== 天气站 UI 组件 =====
    // 状态栏（右上角浮动胶囊）
    lv_obj_t *sensor_label_ = nullptr;      // 左上角温湿度标签
    
    // 时钟卡片（左上）
    lv_obj_t *time_label_ = nullptr;        // 大字时钟 "14:30"
    
    // 日历卡片（右上）
    lv_obj_t *day_label_ = nullptr;         // 星期 "TUE"
    lv_obj_t *date_num_label_ = nullptr;    // 日期 "15"
    lv_obj_t *weather_label_ = nullptr;     // 天气 "晴 25°C"
    lv_obj_t *weather_temp_big_label_ = nullptr;
    lv_obj_t *weather_realfeel_label_ = nullptr;
    lv_obj_t *weather_humidity_label_ = nullptr;
    lv_obj_t *weather_air_label_ = nullptr;
    
    // AI 对话卡片（左下）
    lv_obj_t *chat_card_ = nullptr;         // AI 卡片容器
    lv_obj_t *chat_status_label_ = nullptr; // AI 对话文字（右侧）
    lv_obj_t *emotion_label_ = nullptr;     // 表情文字（左侧下方）
    lv_obj_t *emotion_img_ = nullptr;       // 表情图片（左侧上方，小智自带 emoji）

    // 备忘录卡片（右下）
    lv_obj_t *memo_list_label_ = nullptr;     // 多行备忘列表文字
    lv_obj_t *music_title_label_ = nullptr;   // 音乐页面：歌名
    lv_obj_t *music_artist_label_ = nullptr;  // 音乐页面：歌手
    lv_obj_t *music_lyric_prev_label_ = nullptr;  // 音乐页面：上一句歌词（淡色）
    lv_obj_t *music_lyric_label_ = nullptr;       // 音乐页面：当前歌词（醒目）
    lv_obj_t *music_lyric_next_label_ = nullptr;  // 音乐页面：下一句歌词（淡色）
    lv_obj_t *music_progress_bar_ = nullptr;  // 音乐页面：进度条
    lv_obj_t *music_progress_label_ = nullptr; // 音乐页面：进度文本
    lv_obj_t *music_time_label_ = nullptr;    // 音乐页面：左上角时钟
    lv_obj_t *music_sensor_label_ = nullptr;  // 音乐页面：左上角温湿度
    lv_obj_t *music_chat_status_label_ = nullptr; // 音乐页面：小智状态文案
    lv_obj_t *music_emotion_label_ = nullptr; // 音乐页面：小智情绪标签
    lv_obj_t *music_emotion_img_ = nullptr;   // 音乐页面：小智表情图片（和天气页 emotion_img_ 对应）

    // ===== 番茄钟 UI 组件 =====
    lv_obj_t *pomo_state_label_ = nullptr;       // 状态文字（"专注中"/"休息中"/"已暂停"）
    lv_obj_t *pomo_countdown_label_ = nullptr;   // 大号倒计时 "25:00"
    lv_obj_t *pomo_progress_bar_ = nullptr;      // 进度条
    lv_obj_t *pomo_info_label_ = nullptr;        // 设定信息（"25分钟 专注 / 5分钟 休息"）
    lv_obj_t *pomo_time_label_ = nullptr;        // 顶部时钟
    lv_obj_t *pomo_sensor_label_ = nullptr;      // 顶部温湿度
    lv_obj_t *pomo_chat_status_label_ = nullptr; // AI 状态文字
    lv_obj_t *pomo_emotion_label_ = nullptr;     // 情绪文字
    lv_obj_t *pomo_emotion_img_ = nullptr;       // 表情图片
    lv_obj_t *pomo_wifi_icon_img_ = nullptr;     // 状态栏 WiFi 图标
    lv_obj_t *pomo_battery_icon_img_ = nullptr;  // 状态栏电池图标
    lv_obj_t *pomo_battery_pct_label_ = nullptr; // 状态栏电量文字

    // ===== Pencil 设计各页面 AI 状态栏标签 =====
    // 这些是 AiBarHandles::status_label 的别名，保留以兼容旧代码（SetChatMessage / ClearChatMessages）
    lv_obj_t *weather_ai_status_label_ = nullptr;
    lv_obj_t *quote_ai_status_label_ = nullptr;
    lv_obj_t *photo_ai_status_label_ = nullptr;
    lv_obj_t *pomo_ai_status_label_ = nullptr;
    lv_obj_t *music_ai_status_label_ = nullptr;

    // ===== P0-1：6 桌面 + WiFi QR 页 AI 状态卡的完整 handle =====
    // index: 0=Clock, 1=Weather, 2=Quote, 3=Photo, 4=Pomodoro, 5=Music, 6=WifiQR
    static const int kAiBarCount = 7;
    AiBarHandles ai_bars_[kAiBarCount];
    AiBarStatus current_ai_status_ = AiBarStatus::OFFLINE;
    std::string current_ai_text_ = "AI 待命";
    lv_timer_t* speak_anim_timer_ = nullptr;
    lv_timer_t* pulse_anim_timer_ = nullptr;

    // ===== 新 6 桌面：格言/相册/翻页时钟/音乐 =====
    lv_obj_t *quote_wifi_icon_img_ = nullptr;
    lv_obj_t *quote_battery_icon_img_ = nullptr;
    lv_obj_t *quote_battery_pct_label_ = nullptr;
    lv_obj_t *quote_sensor_label_ = nullptr;
    lv_obj_t *quote_text_label_ = nullptr;       // 格言动态文本

    lv_obj_t *photo_wifi_icon_img_ = nullptr;
    lv_obj_t *photo_battery_icon_img_ = nullptr;
    lv_obj_t *photo_battery_pct_label_ = nullptr;
    lv_obj_t *photo_sensor_label_ = nullptr;

    // ===== 时钟页面：Pencil 设计 1:1 还原 =====
    lv_obj_t *clock_wifi_icon_img_ = nullptr;
    lv_obj_t *clock_battery_icon_img_ = nullptr;
    lv_obj_t *clock_battery_pct_label_ = nullptr;
    lv_obj_t *clock_sensor_label_ = nullptr;
    lv_obj_t *clock_ai_status_label_ = nullptr;    // 顶部左侧：AI 状态文字
    lv_obj_t *clock_digit_[4] = {nullptr};         // 4 位数码管容器 (HH:MM)
    lv_obj_t *clock_digit_segs_[4][7] = {{nullptr}}; // 每位数码管的 7 段 A,B,C,D,E,F,G
    lv_obj_t *clock_colon_dot_top_ = nullptr;      // 冒号上圆点
    lv_obj_t *clock_colon_dot_bot_ = nullptr;      // 冒号下圆点
    lv_obj_t *clock_sec_label_ = nullptr;          // 秒数
    lv_obj_t *clock_date_label_ = nullptr;         // 日期行
    lv_obj_t *clock_temp_label_ = nullptr;         // 大号温度
    lv_obj_t *clock_info_label_ = nullptr;         // 备忘录/天气信息

    // 图片图标（不能用基类的 label，因为我们用 lv_image 而不是 Font Awesome 文字）
    lv_obj_t *wifi_icon_img_ = nullptr;
    lv_obj_t *battery_icon_img_ = nullptr;
    lv_obj_t *battery_pct_label_ = nullptr;  // 电池百分比文字
    lv_obj_t *music_wifi_icon_img_ = nullptr;     // 音乐页面状态栏 WiFi 图标
    lv_obj_t *music_battery_icon_img_ = nullptr;  // 音乐页面状态栏电池图标
    lv_obj_t *music_battery_pct_label_ = nullptr; // 音乐页面状态栏电量文字

    // 数据更新任务句柄
    TaskHandle_t update_task_handle_ = nullptr;
    
    // 系统信息滚动标志（为 true 时暂停 DataUpdateTask 更新，避免锁竞争）
    std::atomic<bool> showing_system_info_{false};
    
    // 省电模式：5 分钟无活动后降低刷新频率（1秒 → 5秒）
    std::atomic<bool> power_saving_{false};     // 是否处于省电模式
    uint32_t last_activity_ms_ = 0;             // 上次用户活动的时间（tick 毫秒）
    static const uint32_t IDLE_TIMEOUT_MS = 5 * 60 * 1000;  // 5 分钟无活动进入省电
    static const uint32_t AUTO_HOME_TIMEOUT_MS = 60 * 1000; // 60 秒无操作回到时钟页
    static const int NORMAL_REFRESH_MS = 1000;  // 正常刷新间隔 1 秒
    static const int SAVING_REFRESH_MS = 5000;  // 省电刷新间隔 5 秒
    
    // 上次更新的值（用于避免不必要的 UI 刷新）
    int last_min_ = -1;
    time_t last_valid_epoch_ = 0;  // NTP 同步后记录正确的 epoch，用于检测时间被外部篡改
    float last_temp_ = -99.0f;
    float last_humi_ = -99.0f;

    // LVGL flush 回调（将 RGB565 转换为 1-bit 并刷新到 RLCD）
    static void Lvgl_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * color_p);

    // UI 创建
    void SetupWeatherUI();
    void SetupMusicUI();
    void SetupPomodoroUI();
    void SetupQuoteUI();
    void SetupPhotoDesktopUI();
    void SetupClockUI();
    void ApplyDisplayMode();

    // 7 段数码管：设置一位数字 (0-9)
    void SetClockDigit(int pos, int value);

    // ===== P0-1 / P0-3：AI 状态卡构建 + 状态推送 + 动画 =====
    // 在 parent (page) 内 (x,y) 处构建一张 220x32 的 AI 状态卡，
    // dark=true 表示音乐页（黑底白字）。bar_index 用于注册到 ai_bars_[]。
    void BuildAiBar(lv_obj_t* parent, int x, int y, int w, int bar_index, bool dark);

    // 推送状态到所有 AI 状态卡（icon 形态 + 默认文案 + 动画启停）。
    // 同时记录 current_ai_status_，便于按需查询。
    void SetAiBarStatusAll(AiBarStatus status);

    // 临时覆盖文案（SetChatMessage 等）。下次 SetAiBarStatusAll 会被覆盖回默认。
    void SetAiBarTextAll(const char* text);

    // 内部：将单张卡设置为指定状态（应用 icon 显隐 + badge）
    void ApplyAiBarStatus(AiBarHandles& h, AiBarStatus status);

    // 动画 timer：聆听脉冲、说话音柱
    static void PulseAnimTimerCb(lv_timer_t* timer);
    static void SpeakAnimTimerCb(lv_timer_t* timer);
    void StartListeningAnim();
    void StopListeningAnim();
    void StartSpeakingAnim();
    void StopSpeakingAnim();
    
    // 备忘录
    void LoadMemoFromNvs();   // 从 NVS 加载备忘录到 UI

    // ===== P0-3：底部提示「按 BOOT 说话 / 单按 USER 切换页面」=====
    lv_obj_t* boot_hint_label_ = nullptr;
    uint32_t boot_hint_show_until_ms_ = 0;     // 0 = 始终显示
    bool boot_hint_dismissed_ = false;         // 用户首次按 BOOT 后置 true

    // ===== P1-1：6 桌面页码指示器 =====
    static const int kPageDotCount = 6;
    lv_obj_t* page_dots_[kAiBarCount][kPageDotCount] = {{nullptr}};
    void BuildPageDots(lv_obj_t* parent, int page_index);
    void RefreshPageDots();

    // ===== P3-1：省电模式月牙图标 =====
    lv_obj_t* power_save_icon_[kAiBarCount] = {nullptr};
    bool last_power_save_drawn_ = false;
    void BuildPowerSaveIcon(lv_obj_t* parent, int page_index);
    void RefreshPowerSaveIcon();

    // ===== P3-4：设置 / 关于 模态层 =====
    lv_obj_t* settings_overlay_ = nullptr;
    lv_obj_t* settings_info_label_ = nullptr;
    void BuildSettingsOverlay();
    void RefreshSettingsOverlay();          // 由 settings 页打开时/USER 单击时调用

    // ===== P3-5：页面切换过渡动画 =====
    void PlayPageTransition(lv_obj_t* prev_page, lv_obj_t* next_page);

    // 数据更新任务（实现在 data_update_task.cc）
    static void DataUpdateTask(void *arg);

public:
    CustomLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy, spi_display_config_t spiconfig, spi_host_device_t spi_host = SPI3_HOST);
    ~CustomLcdDisplay();

    // 获取 RLCD 驱动（供外部调用硬件方法，如对比度调节）
    RlcdDriver* rlcd() const { return rlcd_; }
    
    // 获取 AI 对话标签（供 CustomBoard 设置滚动模式显示系统信息等）
    lv_obj_t* GetChatStatusLabel() const { return chat_status_label_; }
    
    // 系统信息滚动控制（供 CustomBoard 设置标志，避免 DataUpdateTask 锁竞争）
    void SetShowingSystemInfo(bool showing) { showing_system_info_ = showing; }
    
    // 省电模式：记录用户活动，唤醒省电模式
    void NotifyUserActivity();
    bool IsPowerSaving() const { return power_saving_; }
    
    // 重写小智的 AI 显示方法，适配到左下角卡片
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetEmotion(const char* emotion) override;
    virtual void ClearChatMessages() override;
    virtual void ShowWifiProvisioningQr(const char* ssid, const char* url) override;
    virtual void HideWifiProvisioningQr() override;
    
    // 重写状态栏更新（我们用图片图标，不用 Font Awesome 文字）
    virtual void UpdateStatusBar(bool update_all = false) override;
    
    // 重写主题切换（RLCD 单色屏不需要主题切换，避免基类操作不存在的控件导致崩溃）
    virtual void SetTheme(Theme* theme) override;
    virtual void SetMusicInfo(const char* title, const char* artist) override;
    virtual void SetMusicLyric(const char* lyric) override;
    virtual void SetMusicProgress(uint32_t current_ms, uint32_t total_ms) override;
    virtual void SwitchToMusicPage() override;
    virtual void SwitchToWeatherPage() override;
    
    // 启动数据更新任务（需要在网络连接后调用）
    void StartDataUpdateTask();
    
    // 刷新右下角备忘录列表显示（从 NVS 读取后格式化显示）
    void RefreshMemoDisplay();           // 自动获取锁（外部调用用这个）
    void RefreshMemoDisplayInternal();   // 不获取锁（已持锁时用这个，避免死锁）
    void CycleDisplayMode();
    void CycleDisplayModeReverse();         // P1-1：USER 长按反向翻页
    void ToggleSettingsOverlay();           // P3-4：BOOT 长按切换设置 / 关于 modal
    void DismissBootHint();                 // P0-3：用户已学会 BOOT 单击说话后调用，永久隐藏底部提示
    bool IsMusicMode() const { return display_mode_ == MODE_MUSIC; }
    bool IsPomodoroMode() const { return display_mode_ == MODE_POMODORO; }
    bool IsPhotoMode() const { return display_mode_ == MODE_PHOTO; }
    bool IsClockMode() const { return display_mode_ == MODE_CLOCK; }
    bool IsWeatherMode() const { return display_mode_ == MODE_WEATHER; }
    void SwitchToPomodoroPage();
    void SwitchToPhotoPage();
    void SwitchToQuotePage();
    void SwitchToClockPage();

    // 更新相册桌面状态（有图显示正常 UI，无图显示 Web 上传地址）
    void UpdatePhotoDesktopStatus();
    
    // 更新相册图片显示
    void UpdatePhotoImage();
    
    // 更新格言桌面文本
    void UpdateQuoteText(const char* text);

    // 番茄钟 UI 更新方法
    void UpdatePomodoroDisplay(const char* state_text, const char* countdown_text,
                               int progress_permille, const char* info_text);
};

#endif
