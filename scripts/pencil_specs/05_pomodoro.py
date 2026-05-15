"""Spec for pomodoro desktop, derived from
pomodoro_ui.cc :: SetupPomodoroUI().
"""

SPEC = {
    "name": "5. Pomodoro Screen",
    "page_id": "page_pomodoro",
    "width": 400,
    "height": 300,
    "elements": [
        # === Header: AI bar (0,0,220x32) + double 1-px seps + mini status (224,0,175x30) ===
        # pomodoro_ui.cc :: BuildAiBar(page, 0, 0, 220, 4, false)
        {"type": "ref", "ref_id": "HVJtj", "x": 0, "y": 0, "w": 220, "h": 32,
         "name": "aiBar"},
        # pomodoro_ui.cc :: PomoStatusRight(page, ...) → bar at (224, 0, 175x30)
        {"type": "ref", "ref_id": "UaGTC", "x": 224, "y": 4, "w": 175,
         "name": "miniStatus"},
        # Double header separators (LineRect at y=32 and y=34, each 400x1)
        {"type": "rect", "x": 0, "y": 32, "w": 400, "h": 1,
         "opacity": 0.15, "name": "headerSep1"},
        {"type": "rect", "x": 0, "y": 34, "w": 400, "h": 1,
         "opacity": 0.15, "name": "headerSep2"},

        # === Top tagline (font 16px, centered, opacity ~0.75) at y=52 ===
        # pomodoro_ui.cc :: Label(page, "FOCUS ON THE NOW", ..., 0, 52, 400)
        {"type": "label", "x": 0, "y": 52, "w": 400,
         "text": "FOCUS ON THE NOW",
         "size": 16, "align": "center", "weight": "normal", "opacity": 0.75,
         "name": "taglineLabel"},

        # === Central dashed-border box at (80, 88, 240x160) ===
        # Pencil design: dashed stroke, radius=0
        {"type": "hollow", "x": 80, "y": 88, "w": 240, "h": 160,
         "stroke": 1, "stroke_color": "#000000",
         "stroke_dashed": True, "dash_array": [6, 4], "radius": 0,
         "name": "focusBox"},

        # === Countdown label "25:00" (size 48 bold), centered in box ===
        # pomodoro_ui.cc :: Label(page, "25:00", &alibaba_black_64, 80, 120, 240)
        {"type": "label", "x": 80, "y": 120, "w": 240, "text": "25:00",
         "size": 48, "align": "center", "weight": "bold",
         "name": "countdownLabel"},

        # === Start Focus pill button (128x36, radius 18) centered in box ===
        # pomodoro_ui.cc :: Obj(page, 136, 200, 128, 36, white, border, 18)
        # + Label(start, "Start Focus", &font_puhui_16_4, 0, 8, 128)
        {"type": "frame", "x": 136, "y": 200, "w": 128, "h": 36,
         "fill": "#FFFFFF",
         "stroke": {"thickness": 1, "fill": "#000000", "align": "inside"},
         "radius": 18,
         "name": "startButton",
         "children": [
             {"type": "label", "x": 0, "y": 10, "w": 128, "text": "Start Focus",
              "size": 14, "align": "center", "weight": "bold",
              "name": "startLabel"},
         ]},

        # === Bottom hint (font 14px, opacity 0.55, centered) at y=262 ===
        # pomodoro_ui.cc :: Label(page, "双击 USER 键开始专注", ..., 0, 262, 400)
        {"type": "label", "x": 0, "y": 262, "w": 400,
         "text": "双击 USER 键开始专注",
         "size": 14, "align": "center", "opacity": 0.55,
         "name": "hintLabel"},

        # === Page-indicator dots (BuildPageDots, MODE_POMODORO = 4th of 5) ===
        # 5 dots, 6x6, 12px center-to-center, horizontally centered at bottom.
        # quote → photo → weather → pomodoro → clock; pomodoro (4th) is filled.
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
         "fill": "#000000", "name": "pageDot4Active"},
        {"type": "ellipse", "x": 221, "y": 290, "w": 6, "h": 6,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "pageDot5"},
    ],
}
