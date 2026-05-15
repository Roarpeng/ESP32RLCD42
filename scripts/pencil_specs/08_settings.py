"""Spec for settings/about modal overlay, derived from
custom_lcd_display.cc :: BuildSettingsOverlay().
"""

SPEC = {
    "name": "8. Settings Overlay",
    "page_id": "page_settings",
    "width": 400,
    "height": 300,
    "elements": [
        # === Title bar: solid black 400x36 at (0,0) ===
        # lv_obj_create(settings_overlay_) -> 400x36, lv_color_black(), border 0, radius 0.
        {"type": "rect", "x": 0, "y": 0, "w": 400, "h": 36,
         "fill": "#000000", "radius": 0, "name": "titleBar"},

        # Centered white title at y=8 inside the bar.
        # lv_label_create(title_bar) at (0,8) w=400, font_puhui_16_4, white text.
        # Rendered slightly larger/bolder in the design to read as a page title.
        {"type": "label", "x": 0, "y": 8, "w": 400, "text": "设置 · 关于",
         "size": 20, "weight": "bold", "align": "center",
         "fill": "#FFFFFF", "opacity": 1.0, "name": "titleLabel"},

        # === Info area ===
        # Real runtime renders a single multi-line lv_label at (16, 48), w=368,
        # font_puhui_14_1. The snprintf packs 4 key:value rows + a blank line +
        # 2 instruction lines. In the Pencil mockup each visible line becomes its
        # own element so the layout reads cleanly as a settings panel.
        #
        # Row 1: 状态 / state_text  (y=48)
        {"type": "label", "x": 16, "y": 48, "w": 140, "text": "状态",
         "size": 14, "align": "left", "opacity": 0.85,
         "name": "rowStateKey"},
        {"type": "label", "x": 164, "y": 48, "w": 220,
         "text": "在线 · 待命", "size": 14, "align": "right",
         "opacity": 0.85, "name": "rowStateValue"},

        # Row 2: IP / ip address  (y=72)
        {"type": "label", "x": 16, "y": 72, "w": 140, "text": "IP",
         "size": 14, "align": "left", "opacity": 0.7,
         "name": "rowIpKey"},
        {"type": "label", "x": 164, "y": 72, "w": 220,
         "text": "192.168.4.1", "size": 14, "align": "right",
         "opacity": 0.7, "name": "rowIpValue"},

        # Row 3: AI 后端 / protocol_name  (y=96)
        {"type": "label", "x": 16, "y": 96, "w": 140, "text": "AI 后端",
         "size": 14, "align": "left", "opacity": 0.85,
         "name": "rowProtoKey"},
        {"type": "label", "x": 164, "y": 96, "w": 220,
         "text": "WebSocket", "size": 14, "align": "right",
         "opacity": 0.85, "name": "rowProtoValue"},

        # Row 4: OTA / ota_url or default  (y=120)
        {"type": "label", "x": 16, "y": 120, "w": 140, "text": "OTA",
         "size": 14, "align": "left", "opacity": 0.7,
         "name": "rowOtaKey"},
        {"type": "label", "x": 164, "y": 120, "w": 220,
         "text": "默认 (Xiaozhi)", "size": 14, "align": "right",
         "opacity": 0.7, "name": "rowOtaValue"},

        # Thin separator above the action hint block.
        {"type": "line", "x": 16, "y": 154, "w": 368, "opacity": 0.15,
         "name": "infoSep"},

        # Instruction lines below the blank gap, left-aligned in the info area.
        # Same snprintf:  "USER 单击 → 重新配网\nBOOT 长按 → 返回主页"
        {"type": "label", "x": 16, "y": 168, "w": 368,
         "text": "USER 单击 → 重新配网",
         "size": 14, "align": "left", "opacity": 0.8,
         "name": "actionUser"},
        {"type": "label", "x": 16, "y": 192, "w": 368,
         "text": "BOOT 长按 → 返回主页",
         "size": 14, "align": "left", "opacity": 0.8,
         "name": "actionBoot"},

        # === Bottom dismiss hint at y=264, full width, centered, opacity 0.55 ===
        # lv_label at (0,264) w=400, font_puhui_14_1, text_opa = 255 * 0.55.
        {"type": "label", "x": 0, "y": 264, "w": 400,
         "text": "BOOT 长按返回 · USER 单击重新配网",
         "size": 14, "align": "center", "opacity": 0.55,
         "name": "dismissHint"},
    ],
}
