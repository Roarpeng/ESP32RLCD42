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

    // --- Dark AI Bar (x=0, y=0, w=220, h=32, black bg, white text) ---
    lv_obj_t* ai_bar = lv_obj_create(page);
    lv_obj_set_pos(ai_bar, 0, 0);
    lv_obj_set_size(ai_bar, 220, 32);
    lv_obj_set_style_bg_color(ai_bar, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ai_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ai_bar, 0, 0);
    lv_obj_set_style_radius(ai_bar, 0, 0);
    lv_obj_set_style_pad_all(ai_bar, 0, 0);
    lv_obj_remove_flag(ai_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* bot = lv_label_create(ai_bar);
    lv_obj_set_pos(bot, 8, 6);
    lv_obj_set_style_text_font(bot, &font_puhui_16_4, 0);
    lv_obj_set_style_text_color(bot, lv_color_white(), 0);
    lv_label_set_text(bot, "●");

    lv_obj_t* div = lv_obj_create(ai_bar);
    lv_obj_set_pos(div, 36, 6);
    lv_obj_set_size(div, 2, 20);
    lv_obj_set_style_bg_color(div, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(div, (lv_opa_t)(255 * 0.2), 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_set_style_pad_all(div, 0, 0);
    lv_obj_remove_flag(div, LV_OBJ_FLAG_SCROLLABLE);

    music_ai_status_label_ = lv_label_create(ai_bar);
    lv_obj_set_pos(music_ai_status_label_, 46, 6);
    lv_obj_set_width(music_ai_status_label_, 170);
    lv_obj_set_style_text_font(music_ai_status_label_, &font_puhui_16_4, 0);
    lv_obj_set_style_text_color(music_ai_status_label_, lv_color_white(), 0);
    lv_obj_set_style_text_opa(music_ai_status_label_, (lv_opa_t)(255 * 0.7), 0);
    lv_label_set_long_mode(music_ai_status_label_, LV_LABEL_LONG_DOT);
    lv_label_set_text(music_ai_status_label_, "AI 待命");

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
    lv_obj_t* sep1 = lv_obj_create(page);
    lv_obj_set_pos(sep1, 0, 32);
    lv_obj_set_size(sep1, 400, 1);
    lv_obj_set_style_bg_color(sep1, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(sep1, (lv_opa_t)(255 * 0.15), 0);
    lv_obj_set_style_border_width(sep1, 0, 0);
    lv_obj_remove_flag(sep1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* sep2 = lv_obj_create(page);
    lv_obj_set_pos(sep2, 0, 34);
    lv_obj_set_size(sep2, 400, 1);
    lv_obj_set_style_bg_color(sep2, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(sep2, (lv_opa_t)(255 * 0.12), 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_remove_flag(sep2, LV_OBJ_FLAG_SCROLLABLE);

    // ============================================================
    // Vinyl card: x=8, y=44, w=140, h=140, white bg, rounded 14
    // ============================================================
    lv_obj_t *vinyl_card = lv_obj_create(page);
    lv_obj_set_size(vinyl_card, 140, 140);
    lv_obj_set_pos(vinyl_card, 8, 44);
    lv_obj_set_style_bg_color(vinyl_card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(vinyl_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vinyl_card, 0, 0);
    lv_obj_set_style_radius(vinyl_card, 14, 0);
    lv_obj_set_style_pad_all(vinyl_card, 0, 0);
    lv_obj_remove_flag(vinyl_card, LV_OBJ_FLAG_SCROLLABLE);

    // Inner: 130x130 black frame with concentric circles
    lv_obj_t *vinyl_disc = lv_obj_create(vinyl_card);
    lv_obj_set_size(vinyl_disc, 130, 130);
    lv_obj_center(vinyl_disc);
    lv_obj_set_style_radius(vinyl_disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(vinyl_disc, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(vinyl_disc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vinyl_disc, 0, 0);
    lv_obj_set_style_pad_all(vinyl_disc, 0, 0);
    lv_obj_remove_flag(vinyl_disc, LV_OBJ_FLAG_SCROLLABLE);

    const int ring_sizes[] = {104, 84, 64, 44};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *ring = lv_obj_create(vinyl_disc);
        lv_obj_set_size(ring, ring_sizes[i], ring_sizes[i]);
        lv_obj_center(ring);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(ring, 1, 0);
        lv_obj_set_style_border_color(ring, lv_color_white(), 0);
        lv_obj_set_style_border_opa(ring, LV_OPA_30, 0);
        lv_obj_remove_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_obj_t *vinyl_center = lv_obj_create(vinyl_disc);
    lv_obj_set_size(vinyl_center, 24, 24);
    lv_obj_center(vinyl_center);
    lv_obj_set_style_radius(vinyl_center, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(vinyl_center, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(vinyl_center, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vinyl_center, 0, 0);
    lv_obj_remove_flag(vinyl_center, LV_OBJ_FLAG_SCROLLABLE);

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
