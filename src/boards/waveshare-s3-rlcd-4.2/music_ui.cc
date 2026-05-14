// 音乐页 UI —— Pencil 设计 1:1 还原（黑底白字）
//
// 400x300 黑白单色 RLCD，唯一的深色主题页面
// 布局：
// ┌──────────────────────────────────────────┐ BLACK
// │ AI Bar (dark)      │  Status (dark)      │
// ├─────────────────── ├ ────────────────────┤
// │ ┌─────────┐  ┌───────────────────────┐  │
// │ │ Vinyl   │  │ Song title            │  │
// │ │  Card   │  │ Artist                │  │
// │ │ 140x140 │  │ ──────                │  │
// │ │         │  │ prev lyric            │  │
// │ └─────────┘  │ > current lyric       │  │
// │              │ next lyric            │  │
// │              └───────────────────────┘  │
// │ ▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░ 00:00 / 00:00│
// └──────────────────────────────────────────┘

#include "custom_lcd_display.h"
#include <esp_log.h>

LV_FONT_DECLARE(alibaba_puhui_16);
LV_FONT_DECLARE(alibaba_puhui_24);
LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_puhui_14_1);

LV_IMAGE_DECLARE(ui_img_wifi_off);
LV_IMAGE_DECLARE(ui_img_battery_full);

static const char *TAG = "MusicUI";

void CustomLcdDisplay::SetupMusicUI() {
    DisplayLockGuard lock(this);

    lv_obj_t *root = lv_screen_active();

    // ===== Music page container (full screen, BLACK background, hidden) =====
    music_page_ = lv_obj_create(root);
    lv_obj_set_size(music_page_, 400, 300);
    lv_obj_set_pos(music_page_, 0, 0);
    lv_obj_set_style_bg_color(music_page_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(music_page_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(music_page_, 0, 0);
    lv_obj_set_style_pad_all(music_page_, 0, 0);
    lv_obj_set_style_radius(music_page_, 0, 0);
    lv_obj_remove_flag(music_page_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(music_page_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *page = music_page_;

    // ============================================================
    // Top bar: AI Bar (dark variant) + Status (dark variant) + seps
    // ============================================================

    // P0-1：统一改用 BuildAiBar（dark=true 走黑底白字变体）
    BuildAiBar(page, 0, 0, 220, /*bar_index=*/5, /*dark=*/true);  // MODE_MUSIC=5
    music_ai_status_label_ = ai_bars_[5].status_label;

    // --- Dark Status Bar (x=224, y=0, w=175, h=30, white text) ---
    lv_obj_t* st_bar = lv_obj_create(page);
    lv_obj_set_pos(st_bar, 224, 0);
    lv_obj_set_size(st_bar, 175, 30);
    lv_obj_set_style_bg_color(st_bar, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(st_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(st_bar, 0, 0);
    lv_obj_set_style_pad_all(st_bar, 0, 0);
    lv_obj_remove_flag(st_bar, LV_OBJ_FLAG_SCROLLABLE);

    music_wifi_icon_img_ = lv_image_create(st_bar);
    lv_image_set_src(music_wifi_icon_img_, &ui_img_wifi_off);
    lv_obj_set_pos(music_wifi_icon_img_, 0, 6);

    music_battery_icon_img_ = lv_image_create(st_bar);
    lv_image_set_src(music_battery_icon_img_, &ui_img_battery_full);
    lv_obj_set_pos(music_battery_icon_img_, 24, 6);

    music_battery_pct_label_ = lv_label_create(st_bar);
    lv_obj_set_pos(music_battery_pct_label_, 44, 6);
    lv_obj_set_width(music_battery_pct_label_, 28);
    lv_obj_set_style_text_font(music_battery_pct_label_, &alibaba_puhui_16, 0);
    lv_obj_set_style_text_color(music_battery_pct_label_, lv_color_white(), 0);
    lv_label_set_text(music_battery_pct_label_, "85%");

    music_sensor_label_ = lv_label_create(st_bar);
    lv_obj_set_pos(music_sensor_label_, 72, 6);
    lv_obj_set_width(music_sensor_label_, 44);
    lv_obj_set_style_text_font(music_sensor_label_, &alibaba_puhui_16, 0);
    lv_obj_set_style_text_color(music_sensor_label_, lv_color_white(), 0);
    lv_label_set_text(music_sensor_label_, "26.5°C");

    lv_obj_t* humi_lbl = lv_label_create(st_bar);
    lv_obj_set_pos(humi_lbl, 120, 6);
    lv_obj_set_width(humi_lbl, 24);
    lv_obj_set_style_text_font(humi_lbl, &alibaba_puhui_16, 0);
    lv_obj_set_style_text_color(humi_lbl, lv_color_white(), 0);
    lv_label_set_text(humi_lbl, "58%");

    // Dark header separators (white lines on black)
    auto make_dark_sep = [&](int y_pos) {
        lv_obj_t* sep = lv_obj_create(page);
        lv_obj_set_pos(sep, 0, y_pos);
        lv_obj_set_size(sep, 400, 1);
        lv_obj_set_style_bg_color(sep, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(sep, 0, 0);
        lv_obj_set_style_pad_all(sep, 0, 0);
        lv_obj_remove_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
    };
    make_dark_sep(32);
    make_dark_sep(34);

    // P1-1：6 桌面页码指示 + P3-1：省电图标（深色变体）
    BuildPageDots(page, 5);
    BuildPowerSaveIcon(page, 5);

    // ============================================================
    // Vinyl card: x=8, y=44, w=140, h=140, white bg, rounded 14
    // Pencil: 卡片有 2px 白色边框（在黑色页面背景上勾勒卡片轮廓）
    // ============================================================
    lv_obj_t *vinyl_card = lv_obj_create(page);
    lv_obj_set_size(vinyl_card, 140, 140);
    lv_obj_set_pos(vinyl_card, 8, 44);
    lv_obj_set_style_bg_color(vinyl_card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(vinyl_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vinyl_card, 2, 0);
    lv_obj_set_style_border_color(vinyl_card, lv_color_white(), 0);
    lv_obj_set_style_radius(vinyl_card, 14, 0);
    lv_obj_set_style_pad_all(vinyl_card, 0, 0);
    lv_obj_remove_flag(vinyl_card, LV_OBJ_FLAG_SCROLLABLE);

    // Inner: 130x130 black frame with concentric rings
    // Pencil 中 vinylFrame 不设 cornerRadius (方形)，但视觉上仍是黑唱片于白卡内。
    // 为更贴近真实唱片观感，圆角化为圆形仍然合理；按 Pencil 严格还原则使用方形。
    // 这里按 Pencil 还原为方形带 2px 白色边框。
    lv_obj_t *vinyl_disc = lv_obj_create(vinyl_card);
    lv_obj_set_size(vinyl_disc, 130, 130);
    lv_obj_center(vinyl_disc);
    lv_obj_set_style_radius(vinyl_disc, 0, 0);
    lv_obj_set_style_bg_color(vinyl_disc, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(vinyl_disc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vinyl_disc, 2, 0);
    lv_obj_set_style_border_color(vinyl_disc, lv_color_white(), 0);
    lv_obj_set_style_pad_all(vinyl_disc, 0, 0);
    lv_obj_remove_flag(vinyl_disc, LV_OBJ_FLAG_SCROLLABLE);

    // Pencil: 3 个同心圆环 (104/84/64) 在黑色方框内，1px 白色描边
    const int ring_sizes[] = {104, 84, 64};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *ring = lv_obj_create(vinyl_disc);
        lv_obj_set_size(ring, ring_sizes[i], ring_sizes[i]);
        lv_obj_center(ring);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(ring, 1, 0);
        lv_obj_set_style_border_color(ring, lv_color_white(), 0);
        lv_obj_remove_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    }

    // Pencil "vc": 40x40 白色圆盘 + 2px 黑色描边 (唱片中心标签)
    lv_obj_t *vinyl_center = lv_obj_create(vinyl_disc);
    lv_obj_set_size(vinyl_center, 40, 40);
    lv_obj_center(vinyl_center);
    lv_obj_set_style_radius(vinyl_center, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(vinyl_center, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(vinyl_center, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vinyl_center, 2, 0);
    lv_obj_set_style_border_color(vinyl_center, lv_color_black(), 0);
    lv_obj_set_style_pad_all(vinyl_center, 0, 0);
    lv_obj_remove_flag(vinyl_center, LV_OBJ_FLAG_SCROLLABLE);

    // Pencil "vh": 10x10 黑色圆点 (中心轴孔)
    lv_obj_t *vinyl_hole = lv_obj_create(vinyl_center);
    lv_obj_set_size(vinyl_hole, 10, 10);
    lv_obj_center(vinyl_hole);
    lv_obj_set_style_radius(vinyl_hole, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(vinyl_hole, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(vinyl_hole, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vinyl_hole, 0, 0);
    lv_obj_set_style_pad_all(vinyl_hole, 0, 0);
    lv_obj_remove_flag(vinyl_hole, LV_OBJ_FLAG_SCROLLABLE);

    // ============================================================
    // Song card: x=160, y=44, w=232, h=140, white bg, rounded 14
    // ============================================================
    lv_obj_t *info_card = lv_obj_create(page);
    lv_obj_set_size(info_card, 232, 140);
    lv_obj_set_pos(info_card, 160, 44);
    lv_obj_set_style_bg_color(info_card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(info_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(info_card, 0, 0);
    lv_obj_set_style_radius(info_card, 14, 0);
    lv_obj_set_style_pad_all(info_card, 10, 0);
    lv_obj_remove_flag(info_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_clip_corner(info_card, true, 0);

    const int info_w = 212;

    // Song title: font 16px, "未播放"
    music_title_label_ = lv_label_create(info_card);
    lv_obj_set_style_text_font(music_title_label_, &font_puhui_16_4, 0);
    lv_obj_set_style_text_color(music_title_label_, lv_color_black(), 0);
    lv_obj_set_width(music_title_label_, info_w);
    lv_label_set_long_mode(music_title_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(music_title_label_, "未播放");
    lv_obj_align(music_title_label_, LV_ALIGN_TOP_LEFT, 0, 4);

    // Artist: font 14px, opacity=0.6, "未知歌手"
    music_artist_label_ = lv_label_create(info_card);
    lv_obj_set_style_text_font(music_artist_label_, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(music_artist_label_, lv_color_black(), 0);
    lv_obj_set_style_text_opa(music_artist_label_, (lv_opa_t)(255 * 0.6), 0);
    lv_obj_set_width(music_artist_label_, info_w);
    lv_label_set_long_mode(music_artist_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(music_artist_label_, "未知歌手");
    lv_obj_align(music_artist_label_, LV_ALIGN_TOP_LEFT, 0, 26);

    // Separator line: opacity=0.2
    lv_obj_t *info_sep = lv_obj_create(info_card);
    lv_obj_set_size(info_sep, info_w, 1);
    lv_obj_set_style_bg_color(info_sep, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(info_sep, LV_OPA_20, 0);
    lv_obj_set_style_border_width(info_sep, 0, 0);
    lv_obj_align(info_sep, LV_ALIGN_TOP_LEFT, 0, 48);
    lv_obj_remove_flag(info_sep, LV_OBJ_FLAG_SCROLLABLE);

    // Prev lyric: font 14px, opacity=0.4
    music_lyric_prev_label_ = lv_label_create(info_card);
    lv_obj_set_style_text_font(music_lyric_prev_label_, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(music_lyric_prev_label_, lv_color_black(), 0);
    lv_obj_set_style_text_opa(music_lyric_prev_label_, (lv_opa_t)(255 * 0.4), 0);
    lv_obj_set_size(music_lyric_prev_label_, info_w, 22);
    lv_label_set_long_mode(music_lyric_prev_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(music_lyric_prev_label_, "");
    lv_obj_align(music_lyric_prev_label_, LV_ALIGN_TOP_LEFT, 0, 56);

    // Current lyric: font 16px bold, "等待播放..."
    music_lyric_label_ = lv_label_create(info_card);
    lv_obj_set_style_text_font(music_lyric_label_, &font_puhui_16_4, 0);
    lv_obj_set_style_text_color(music_lyric_label_, lv_color_black(), 0);
    lv_obj_set_width(music_lyric_label_, info_w);
    lv_label_set_long_mode(music_lyric_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(music_lyric_label_, "等待播放...");
    lv_obj_align(music_lyric_label_, LV_ALIGN_TOP_LEFT, 0, 80);

    // Next lyric: font 14px, opacity=0.4
    music_lyric_next_label_ = lv_label_create(info_card);
    lv_obj_set_style_text_font(music_lyric_next_label_, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(music_lyric_next_label_, lv_color_black(), 0);
    lv_obj_set_style_text_opa(music_lyric_next_label_, (lv_opa_t)(255 * 0.4), 0);
    lv_obj_set_size(music_lyric_next_label_, info_w, 22);
    lv_label_set_long_mode(music_lyric_next_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(music_lyric_next_label_, "");
    lv_obj_align(music_lyric_next_label_, LV_ALIGN_TOP_LEFT, 0, 104);

    // ============================================================
    // Progress bar: x=12, y=198, w=280, h=12, rounded 6, border 1px
    // ============================================================
    music_progress_bar_ = lv_bar_create(page);
    lv_obj_set_size(music_progress_bar_, 280, 12);
    lv_obj_set_pos(music_progress_bar_, 12, 198);
    lv_bar_set_range(music_progress_bar_, 0, 1000);
    lv_bar_set_value(music_progress_bar_, 0, LV_ANIM_OFF);

    // Track: white bg + 1px border, rounded 6
    lv_obj_set_style_bg_color(music_progress_bar_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(music_progress_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(music_progress_bar_, 1, 0);
    lv_obj_set_style_border_color(music_progress_bar_, lv_color_white(), 0);
    lv_obj_set_style_radius(music_progress_bar_, 6, 0);
    lv_obj_set_style_pad_all(music_progress_bar_, 1, 0);

    // Indicator: black fill, rounded 5
    lv_obj_set_style_bg_color(music_progress_bar_, lv_color_black(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(music_progress_bar_, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(music_progress_bar_, 5, LV_PART_INDICATOR);

    // Progress time: x=296, y=196, font 13px, white, opacity=0.5
    music_progress_label_ = lv_label_create(page);
    lv_obj_set_pos(music_progress_label_, 296, 196);
    lv_obj_set_style_text_font(music_progress_label_, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(music_progress_label_, lv_color_white(), 0);
    lv_obj_set_style_text_opa(music_progress_label_, (lv_opa_t)(255 * 0.5), 0);
    lv_label_set_text(music_progress_label_, "00:00 / 00:00");

    ESP_LOGI(TAG, "音乐页面 UI 创建完成（Pencil 黑底设计）");
}
