"""Spec for WiFi provisioning QR page, derived from
custom_lcd_display.cc :: ShowWifiProvisioningQr().
"""

SPEC = {
    "name": "7. WiFi QR Page",
    "page_id": "page_wifi_qr",
    "width": 400,
    "height": 300,
    "elements": [
        # === Header: AI bar (0,0,220x32) + double 1-px seps + mini status (224,4,175x30) ===
        # BuildAiBar(page, 0, 0, 220, bar_index=6 WIFI_QR, dark=false)
        {"type": "ref", "ref_id": "HVJtj", "x": 0, "y": 0, "w": 220, "h": 32,
         "name": "aiBar"},
        # DesktopHeaderSeps -> DesktopHeaderSepDouble: lines at y=32 and y=34.
        {"type": "rect", "x": 0, "y": 32, "w": 400, "h": 1,
         "opacity": 0.15, "name": "headerSepTop"},
        {"type": "rect", "x": 0, "y": 34, "w": 400, "h": 1,
         "opacity": 0.15, "name": "headerSepBottom"},
        # DesktopStatusRight(page, 224, 4, ...): 175x30 mini status bar.
        {"type": "ref", "ref_id": "UaGTC", "x": 224, "y": 4, "w": 175,
         "name": "miniStatus"},

        # === Centered page title at y=50 (font 20px bold) ===
        {"type": "label", "x": 0, "y": 50, "w": 400, "text": "扫码打开配网页",
         "size": 20, "weight": "bold", "align": "center",
         "name": "qrTitle"},

        # === QR-code frame: 160x160 at (120, 80), 2px black stroke, white fill ===
        # DesktopObj(...) renders an empty rectangle that the runtime fills with
        # the generated QR canvas. In the Pencil mockup we show the frame plus a
        # centered placeholder label.
        {"type": "frame", "x": 120, "y": 80, "w": 160, "h": 160,
         "name": "qrFrame",
         "fill": "#FFFFFF",
         "stroke": {"thickness": 2, "fill": "#000000", "align": "inside"},
         "children": [
             {"type": "label", "x": 0, "y": 64, "w": 160, "text": "QR Code",
              "size": 16, "weight": "bold", "align": "center",
              "name": "qrPlaceholder"},
             {"type": "label", "x": 0, "y": 88, "w": 160, "text": "(170 x 170)",
              "size": 14, "opacity": 0.55, "align": "center",
              "name": "qrPlaceholderSize"},
         ]},

        # === Bottom info lines: hotspot SSID (y=255) and provisioning URL (y=270) ===
        # DesktopLabel(... font_puhui_14_1, 0, 255, 400, ALIGN_CENTER) + opacity overlay.
        {"type": "label", "x": 0, "y": 255, "w": 400,
         "text": "热点: 设备WiFi热点名",
         "size": 14, "align": "center", "opacity": 0.7,
         "name": "ssidLine"},
        {"type": "label", "x": 0, "y": 270, "w": 400,
         "text": "配网页: http://192.168.4.1",
         "size": 14, "align": "center", "opacity": 0.7,
         "name": "urlLine"},
    ],
}
