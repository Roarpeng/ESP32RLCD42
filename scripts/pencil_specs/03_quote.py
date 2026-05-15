"""Spec for quote desktop, derived from
custom_lcd_display.cc :: SetupQuoteUI().
"""

SPEC = {
    "name": "3. Quote Screen",
    "page_id": "page_quote",
    "width": 400,
    "height": 300,
    "elements": [
        # === Header: AI bar (0,0,220x32) + DOUBLE 1-px seps + mini status (224,0,175x30) ===
        {"type": "ref", "ref_id": "HVJtj", "x": 0, "y": 0, "w": 220, "h": 32,
         "name": "aiBar"},
        # Quote page uses DesktopHeaderSepDouble: lines at y=32 AND y=34.
        {"type": "rect", "x": 0, "y": 32, "w": 400, "h": 1,
         "opacity": 0.15, "name": "headerSepTop"},
        {"type": "rect", "x": 0, "y": 34, "w": 400, "h": 1,
         "opacity": 0.15, "name": "headerSepBot"},
        {"type": "ref", "ref_id": "UaGTC", "x": 224, "y": 4, "w": 175,
         "name": "miniStatus"},

        # === Big opening curly quote mark at (30, 56), font 48px, opacity 0.2 ===
        {"type": "label", "x": 30, "y": 56, "w": 80, "text": "\u201C",
         "size": 48, "opacity": 0.2, "name": "quoteMark"},

        # === Quote body text at (50, 86), w=300, font 24px, opacity 0.8 ===
        {"type": "label", "x": 50, "y": 86, "w": 300,
         "text": "Fall seven times,\nstand up eight.",
         "size": 24, "opacity": 0.8, "name": "quoteText"},

        # === NEW QUOTE button: rounded pill 160x42 at (120, 210), border 2px, radius 21
        # Contains a refresh-cw icon (24x24) and a bold 14px label, row-laid out. ===
        {"type": "frame", "x": 120, "y": 210, "w": 160, "h": 42,
         "name": "newQuoteBtn",
         "fill": "#FFFFFF",
         "stroke": {"thickness": 2, "fill": "#000000", "align": "inside"},
         "radius": 21,
         "padding": 16, "gap": 8, "layout": "row",
         "justify": "center", "align_items": "center",
         "children": [
             {"type": "icon", "x": 0, "y": 0, "w": 24, "h": 24,
              "icon": "refresh-cw", "name": "refreshIcon"},
             {"type": "label", "x": 0, "y": 0, "w": 96,
              "text": "NEW QUOTE", "size": 14, "weight": "bold",
              "align": "center", "name": "btnLabel"},
         ]},

        # === Decorative stick-person figure at right edge ===
        # Head: 12x12 circle at (340, 226).
        {"type": "ellipse", "x": 340, "y": 226, "w": 12, "h": 12,
         "fill": "#000000", "name": "personHead"},
        # Body: 2px vertical line at (345, 238), height 20.
        {"type": "rect", "x": 345, "y": 238, "w": 2, "h": 20,
         "fill": "#000000", "name": "personBody"},
        # Arms: 22x2 horizontal line at (335, 248).
        {"type": "rect", "x": 335, "y": 248, "w": 22, "h": 2,
         "fill": "#000000", "name": "personArms"},
        # Left leg: 2x18 vertical line at (340, 258).
        {"type": "rect", "x": 340, "y": 258, "w": 2, "h": 18,
         "fill": "#000000", "name": "personLegL"},
        # Right leg: 2x18 vertical line at (348, 258).
        {"type": "rect", "x": 348, "y": 258, "w": 2, "h": 18,
         "fill": "#000000", "name": "personLegR"},

        # === Page-indicator dots (BuildPageDots, MODE_QUOTE = 1st of 5) ===
        # 5 dots, 6x6, 12px center-to-center, horizontally centered at bottom.
        # quote → photo → weather → pomodoro → clock; quote (leftmost) is filled.
        {"type": "ellipse", "x": 173, "y": 290, "w": 6, "h": 6,
         "fill": "#000000", "name": "pageDot1Active"},
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
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "pageDot5"},
    ],
}
