# Pencil page-spec schema

Each page-spec module under `scripts/pencil_specs/` must expose a `SPEC` dict
with this shape:

```python
SPEC = {
    "name": "1. Clock Screen",   # human-readable frame name
    "page_id": "page_clock",     # short snake-case id (unique across pages)
    "width": 400,
    "height": 300,
    "elements": [
        # ordered list of DSL elements; later elements render on top
        ...
    ],
}
```

The page surface is `400 × 300` (matches the RLCD panel).  All coordinates are
page-local pixels.

## Reusable refs (already defined in pencil-new.pen — just reference them)

| `ref_id` | What it is              | Native size | Notes                                 |
| -------- | ----------------------- | ----------- | ------------------------------------- |
| `HVJtj`  | AI Status Card          | 229×32      | Render with `w=220, h=32`             |
| `UaGTC`  | Mini Status Bar         | 175×30      | WiFi+battery%+temp+humidity row       |
| `b5t5X`  | 7-Seg Digit             | 44×80       | Use `w=57, h=85` to match clock pages |
| `bd3Wx`  | Colon Dots              | 12×80       | Two stacked rounded dots              |
| `xDTeq`  | Corner Deco             | 12×12       | Two perpendicular short lines         |

## Element DSL

All elements support `x`, `y`, and an optional `name`.  Coordinates default
to `0`.

```python
{"type": "ref", "ref_id": "HVJtj", "x": 0, "y": 0, "w": 220, "h": 32,
 "name": "aiBar"}

{"type": "rect", "x": 0, "y": 32, "w": 400, "h": 1, "name": "sep",
 "opacity": 0.15, "fill": "#000000", "radius": 0}

{"type": "line", "x": 10, "y": 228, "w": 380, "name": "infoLine"}
# alias for a 1-pixel rect with opacity 0.15

{"type": "hollow", "x": 80, "y": 88, "w": 240, "h": 160, "name": "dashedBox",
 "stroke": 1, "stroke_color": "#000000", "stroke_dashed": True,
 "dash_array": [6, 4], "radius": 8}

{"type": "label", "x": 0, "y": 195, "w": 400, "text": "06 / 12  周三",
 "size": 20, "align": "center", "weight": "normal", "opacity": 0.8,
 "name": "date"}
# size ∈ {14, 16, 20, 24, 32, 48}; weight ∈ {"normal", "bold"}

{"type": "ellipse", "x": 192, "y": 100, "w": 8, "h": 8, "name": "dot",
 "fill": "#000000"}

{"type": "icon", "x": 40, "y": 72, "w": 54, "h": 54, "icon": "cloud-sun",
 "name": "weatherIcon"}
# Common lucide icons: wifi, wifi-off, battery, battery-charging,
#   thermometer, droplet, map-pin, cloud, cloud-sun, sun, moon, image,
#   refresh-cw, play, pause, qr-code, message-circle, focus, leaf,
#   bar-chart-3, volume-2, music-2

{"type": "frame", "x": 0, "y": 0, "w": 100, "h": 50, "name": "card",
 "fill": "#FFFFFF",
 "stroke": {"thickness": 2, "fill": "#000000", "align": "inside"},
 "radius": 8,
 "padding": 16, "gap": 8, "layout": "row",
 "justify": "center", "align_items": "center",
 "children": [
     # nested DSL elements with frame-local coordinates
 ]}
```

## Drawing conventions used in the codebase

- Page is white background (`#FFFFFF`); foreground is black (`#000000`).
- Single 1-px header separator at `y=32` (`opacity=0.15`) — clock + weather pages.
- Double header separators at `y=32` and `y=34` (each `opacity=0.15`) — quote,
  photo, pomodoro, music, wifi-QR pages.
- Status bar mini widget sits at `(224, 4)` and is 175 wide.
- AI bar sits at `(0, 0)` and is 220 wide × 32 tall.
- Page indicator dots: 5 dots at `y=8`, dots are 6px circles spaced 12px apart.
  The active page is filled, others are outlined.  Not strictly required in the
  spec because they're a small decoration — page specs may omit them if the
  source code shows them in identical positions (parent handles centrally),
  but it is fine to include them for clarity.
- Common fonts:
  - 14px regular (body, memo)
  - 16px regular (status bar values, date, button labels)
  - 20px regular (date headings)
  - 24px regular (large body, big counters)
  - 32px regular (very big temperature, decorative numbers)
  - 48px regular (`alibaba_puhui_48`, big clock temp)

## Worked example (do NOT submit this — it is just a reference)

```python
# scripts/pencil_specs/01_clock.py
"""Spec for clock desktop, derived from
custom_lcd_display.cc :: SetupClockUI().
"""

SPEC = {
    "name": "1. Clock Screen",
    "page_id": "page_clock",
    "width": 400,
    "height": 300,
    "elements": [
        # Header: AI bar (220×32) + single 1-px sep + mini status (175×30)
        {"type": "ref", "ref_id": "HVJtj", "x": 0, "y": 0, "w": 220, "h": 32,
         "name": "aiBar"},
        {"type": "rect", "x": 0, "y": 32, "w": 400, "h": 1,
         "opacity": 0.15, "name": "headerSep"},
        {"type": "ref", "ref_id": "UaGTC", "x": 223, "y": 4, "w": 175,
         "name": "miniStatus"},

        # Four 7-segment digits + a vertical "colon dots" between them.
        {"type": "ref", "ref_id": "b5t5X", "x": 58,  "y": 77, "w": 57, "h": 85,
         "name": "d1"},
        {"type": "ref", "ref_id": "b5t5X", "x": 125, "y": 77, "w": 57, "h": 85,
         "name": "d2"},
        {"type": "ref", "ref_id": "bd3Wx", "x": 192, "y": 77, "w": 15, "h": 85,
         "name": "colon"},
        {"type": "ref", "ref_id": "b5t5X", "x": 217, "y": 77, "w": 57, "h": 85,
         "name": "d3"},
        {"type": "ref", "ref_id": "b5t5X", "x": 284, "y": 77, "w": 57, "h": 85,
         "name": "d4"},

        # Centered date row
        {"type": "label", "x": 0, "y": 195, "w": 400, "text": "06 / 12  周三",
         "size": 20, "align": "center", "name": "dateLabel"},

        # Decorative separator above the bottom info area
        {"type": "line", "x": 10, "y": 228, "w": 380, "opacity": 0.12,
         "name": "infoLine"},

        # Big temperature (left) and memo strip (right)
        {"type": "label", "x": 10, "y": 238, "text": "26.5°C", "size": 32,
         "name": "bigTemp"},
        {"type": "label", "x": 10, "y": 274, "w": 380, "size": 14,
         "opacity": 0.55,
         "text": "08:30 买早餐  |  12:00 午餐会  |  下午交报告",
         "name": "memoText"},

        # P0-3 hint shown until BOOT pressed once
        {"type": "label", "x": 0, "y": 285, "w": 400,
         "text": "按 BOOT 说话 · 单按 USER 切换页面",
         "size": 14, "align": "center", "opacity": 0.55,
         "name": "bootHint"},
    ],
}
```

