"""Spec for weather desktop, derived from
weather_ui.cc :: SetupWeatherUI().
"""

SPEC = {
    "name": "2. Weather Screen",
    "page_id": "page_weather",
    "width": 400,
    "height": 300,
    "elements": [
        # === Header: AI bar (0,0,220x32) + single 1-px sep + mini status (224,4,175x30) ===
        # Weather is one of the two pages using DesktopHeaderSepSingle (clock + weather).
        {"type": "ref", "ref_id": "HVJtj", "x": 0, "y": 0, "w": 220, "h": 32,
         "name": "aiBar"},
        {"type": "rect", "x": 0, "y": 32, "w": 400, "h": 1,
         "opacity": 0.15, "name": "headerSep"},
        {"type": "ref", "ref_id": "UaGTC", "x": 224, "y": 4, "w": 175,
         "name": "miniStatus"},

        # === Lucide cloud-sun icon on the left near the big temperature ===
        # Source uses primitive circles/rects at ~(40,72) to fake the silhouette
        # on the 1-bit panel; the design intent is a 54x54 cloud-sun glyph.
        {"type": "icon", "x": 40, "y": 72, "w": 54, "h": 54,
         "icon": "cloud-sun", "name": "weatherIcon"},

        # === Big temperature: x=100, y=88, font 48px ===
        {"type": "label", "x": 100, "y": 88, "w": 175, "text": "26°C",
         "size": 48, "name": "bigTemp"},

        # === Condition word: x=20, y=190, font 24px ===
        {"type": "label", "x": 20, "y": 190, "w": 150, "text": "多云",
         "size": 24, "name": "condition"},

        # === Location row: map-pin icon (~14x14) + city/district text (24px) ===
        # Source draws the pin as a small circle + tail at (22..30, 250..262);
        # in the spec we collapse that to one map-pin lucide icon.
        {"type": "icon", "x": 22, "y": 247, "w": 14, "h": 14,
         "icon": "map-pin", "name": "locationPin"},
        {"type": "label", "x": 44, "y": 242, "w": 250, "text": "深圳, 南山区",
         "size": 24, "name": "locationText"},

        # === Right-side metrics column (x=290, width 100/70) ===
        # Titles 16px @ opacity 0.5, values 16px regular.
        # 体感温度 / 27°C
        {"type": "label", "x": 290, "y": 70, "w": 100, "text": "体感温度",
         "size": 16, "opacity": 0.5, "name": "realfeelTitle"},
        {"type": "label", "x": 290, "y": 92, "w": 70, "text": "27°C",
         "size": 16, "name": "realfeelValue"},
        # 湿度 / 58%
        {"type": "label", "x": 290, "y": 124, "w": 100, "text": "湿度",
         "size": 16, "opacity": 0.5, "name": "humidityTitle"},
        {"type": "label", "x": 290, "y": 146, "w": 70, "text": "58%",
         "size": 16, "name": "humidityValue"},
        # 空气质量 / 28
        {"type": "label", "x": 290, "y": 178, "w": 100, "text": "空气质量",
         "size": 16, "opacity": 0.5, "name": "airTitle"},
        {"type": "label", "x": 290, "y": 200, "w": 70, "text": "28",
         "size": 16, "name": "airValue"},

        # === Page-indicator dots (BuildPageDots, MODE_WEATHER = 3rd of 5) ===
        # 5 dots, 6x6, 12px center-to-center, horizontally centered at bottom.
        # quote → photo → weather → pomodoro → clock; weather (3rd) is filled.
        {"type": "ellipse", "x": 173, "y": 290, "w": 6, "h": 6,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "pageDot1"},
        {"type": "ellipse", "x": 185, "y": 290, "w": 6, "h": 6,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "pageDot2"},
        {"type": "ellipse", "x": 197, "y": 290, "w": 6, "h": 6,
         "fill": "#000000", "name": "pageDot3Active"},
        {"type": "ellipse", "x": 209, "y": 290, "w": 6, "h": 6,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "pageDot4"},
        {"type": "ellipse", "x": 221, "y": 290, "w": 6, "h": 6,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "pageDot5"},
    ],
}
