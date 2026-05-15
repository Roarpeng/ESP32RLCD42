"""Spec for photo desktop, derived from
custom_lcd_display.cc :: SetupPhotoDesktopUI().
"""

SPEC = {
    "name": "4. Photo Screen",
    "page_id": "page_photo",
    "width": 400,
    "height": 300,
    "elements": [
        # === Header: AI bar (0,0,220x32) + double seps + mini status (224,0,175x30) ===
        {"type": "ref", "ref_id": "HVJtj", "x": 0, "y": 0, "w": 220, "h": 32,
         "name": "aiBar"},
        {"type": "rect", "x": 0, "y": 32, "w": 400, "h": 1,
         "opacity": 0.15, "name": "headerSepTop"},
        {"type": "rect", "x": 0, "y": 34, "w": 400, "h": 1,
         "opacity": 0.15, "name": "headerSepBot"},
        {"type": "ref", "ref_id": "UaGTC", "x": 224, "y": 4, "w": 175,
         "name": "miniStatus"},

        # === Photo frame: 1-px black border, white fill, at (15, 42, 370, 190) ===
        {"type": "hollow", "x": 15, "y": 42, "w": 370, "h": 190,
         "stroke": 1, "stroke_color": "#000000", "radius": 0,
         "name": "photoFrame"},

        # === Empty-state overlay: 4 labels positioned vertically inside the frame ===
        # Title (bold, 20px) — overlay y=40 inside overlay-at-38 -> absolute y=78
        {"type": "label", "x": 15, "y": 78, "w": 370, "text": "相册为空",
         "size": 20, "align": "center", "weight": "bold", "opacity": 0.8,
         "name": "emptyTitle"},
        # Hint line 1 (14px) — overlay y=90 -> absolute y=128
        {"type": "label", "x": 15, "y": 128, "w": 370,
         "text": "请通过 Web 上传照片",
         "size": 14, "align": "center", "opacity": 0.8,
         "name": "emptyHint1"},
        # Upload URL (14px) — overlay y=115 -> absolute y=153
        {"type": "label", "x": 15, "y": 153, "w": 370,
         "text": "http://192.168.4.1",
         "size": 14, "align": "center", "opacity": 0.8,
         "name": "emptyUrl"},
        # Hint line 2 (14px) — overlay y=145 -> absolute y=183
        {"type": "label", "x": 15, "y": 183, "w": 370,
         "text": "支持 JPG/PNG/BMP 格式",
         "size": 14, "align": "center", "opacity": 0.8,
         "name": "emptyHint2"},

        # === Bottom navigation row near y≈268 ===
        {"type": "label", "x": 140, "y": 270, "text": "<",
         "size": 24, "weight": "bold", "name": "navPrev"},
        {"type": "label", "x": 175, "y": 273, "w": 60, "text": "1 / 1",
         "size": 16, "align": "center", "name": "navIndicator"},
        {"type": "label", "x": 225, "y": 270, "text": ">",
         "size": 24, "weight": "bold", "name": "navNext"},

        # === Page-indicator dots (BuildPageDots, MODE_PHOTO = 2nd of 5) ===
        # 5 dots, 6x6, 12px center-to-center, horizontally centered at bottom.
        # quote → photo → weather → pomodoro → clock; photo (2nd) is filled.
        {"type": "ellipse", "x": 173, "y": 290, "w": 6, "h": 6,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "pageDot1"},
        {"type": "ellipse", "x": 185, "y": 290, "w": 6, "h": 6,
         "fill": "#000000", "name": "pageDot2Active"},
        {"type": "ellipse", "x": 197, "y": 290, "w": 6, "h": 6,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "pageDot3"},
        {"type": "ellipse", "x": 209, "y": 290, "w": 6, "h": 6,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "pageDot4"},
        {"type": "ellipse", "x": 221, "y": 290, "w": 6, "h": 6,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "pageDot5"},
    ],
}
