"""Spec for clock desktop, derived from
custom_lcd_display.cc :: SetupClockUI().
"""

SPEC = {
    "name": "1. Clock Screen",
    "page_id": "page_clock",
    "width": 400,
    "height": 300,
    "elements": [
        # === Header: AI bar (0,0,220x32) + single 1-px sep + mini status (224,0,175x30) ===
        {"type": "ref", "ref_id": "HVJtj", "x": 0, "y": 0, "w": 220, "h": 32,
         "name": "aiBar"},
        {"type": "rect", "x": 0, "y": 32, "w": 400, "h": 1,
         "opacity": 0.15, "name": "headerSep"},
        {"type": "ref", "ref_id": "UaGTC", "x": 224, "y": 4, "w": 175,
         "name": "miniStatus"},

        # === Four 7-segment digits at y=77, with colon dots between digits 2 and 3 ===
        {"type": "ref", "ref_id": "b5t5X", "x": 58,  "y": 77, "w": 57, "h": 85,
         "name": "digit1"},
        {"type": "ref", "ref_id": "b5t5X", "x": 125, "y": 77, "w": 57, "h": 85,
         "name": "digit2"},
        {"type": "ref", "ref_id": "bd3Wx", "x": 192, "y": 77, "w": 15, "h": 85,
         "name": "colon"},
        {"type": "ref", "ref_id": "b5t5X", "x": 217, "y": 77, "w": 57, "h": 85,
         "name": "digit3"},
        {"type": "ref", "ref_id": "b5t5X", "x": 284, "y": 77, "w": 57, "h": 85,
         "name": "digit4"},

        # === Centered date row (font 20px) ===
        {"type": "label", "x": 0, "y": 195, "w": 400, "text": "06 / 12  周三",
         "size": 20, "align": "center", "name": "dateLabel"},

        # === Decorative separator above bottom info area (x=10, y=228, w=380) ===
        {"type": "line", "x": 10, "y": 228, "w": 380, "opacity": 0.15,
         "name": "infoLine"},

        # === Big temperature (font 32px) and memo strip (font 14px, opacity 0.55) ===
        {"type": "label", "x": 10, "y": 238, "text": "26.5°C", "size": 32,
         "name": "bigTemp"},
        {"type": "label", "x": 10, "y": 274, "w": 380, "size": 14,
         "opacity": 0.55,
         "text": "08:30 买早餐  |  12:00 午餐会  |  下午交报告",
         "name": "memoText"},

        # === P0-3 boot hint (auto-hidden after first BOOT press) ===
        {"type": "label", "x": 0, "y": 285, "w": 400,
         "text": "按 BOOT 说话 · 单按 USER 切换页面",
         "size": 14, "align": "center", "opacity": 0.55,
         "name": "bootHint"},

        # === Page-indicator dots (BuildPageDots, MODE_CLOCK = 5th of 5) ===
        # 5 dots, 6x6, 12px center-to-center, horizontally centered at bottom.
        # quote → photo → weather → pomodoro → clock; clock (rightmost) is filled.
        {"type": "ellipse", "x": 173, "y": 290, "w": 6, "h": 6,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "pageDot1"},
        {"type": "ellipse", "x": 185, "y": 290, "w": 6, "h": 6,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "pageDot2"},
        {"type": "ellipse", "x": 197, "y": 290, "w": 6, "h": 6,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "pageDot3"},
        {"type": "ellipse", "x": 209, "y": 290, "w": 6, "h": 6,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "pageDot4"},
        {"type": "ellipse", "x": 221, "y": 290, "w": 6, "h": 6,
         "fill": "#000000", "name": "pageDot5Active"},
    ],
}
