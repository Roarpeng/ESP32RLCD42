# Pencil page specs

This folder holds one Python module per UI page in the
`waveshare-s3-rlcd-4.2` firmware.  Each module exports a single `SPEC` dict
describing the page layout in a small DSL — see `_schema.md` for the full
schema and a worked example.

The builder at `scripts/build_pencil.py` aggregates every `SPEC` here with the
reusable components already present in `pencil-new.pen`, and rewrites
`pencil-new.pen` end-to-end.

## How to regenerate `pencil-new.pen`

```bash
python3 scripts/build_pencil.py
```

The script preserves the reusable components (`Corner Deco`, `AI Status Card`,
`Mini Status Bar`, `7-Seg Digit`, `Colon Dots`) verbatim and lays out the pages
horizontally with a 420-pixel stride starting at `x = -7`, matching Pencil's
default placement for new frames.

## Files

| File              | Page                | Derived from                                                     |
| ----------------- | ------------------- | ---------------------------------------------------------------- |
| `01_clock.py`     | 1. Clock Screen     | `custom_lcd_display.cc :: SetupClockUI()`                        |
| `02_weather.py`   | 2. Weather Screen   | `weather_ui.cc :: SetupWeatherUI()`                              |
| `03_quote.py`     | 3. Quote Screen     | `custom_lcd_display.cc :: SetupQuoteUI()`                        |
| `04_photo.py`     | 4. Photo Screen     | `custom_lcd_display.cc :: SetupPhotoDesktopUI()`                 |
| `05_pomodoro.py`  | 5. Pomodoro Screen  | `pomodoro_ui.cc :: SetupPomodoroUI()`                            |
| `06_music.py`     | 6. Music Player     | `music_ui.cc :: SetupMusicUI()`                                  |
| `07_wifi_qr.py`   | 7. WiFi QR Page     | `custom_lcd_display.cc :: ShowWifiProvisioningQr()`              |
| `08_settings.py`  | 8. Settings Overlay | `custom_lcd_display.cc :: BuildSettingsOverlay()`                |

## Adding a new page

1. Create `scripts/pencil_specs/NN_pagename.py` exporting a `SPEC` dict.
2. Add a unique 2-letter prefix entry in `_PAGE_PREFIX` in
   `scripts/build_pencil.py` to keep generated element IDs distinct from other
   pages.
3. Run `python3 scripts/build_pencil.py`.
4. Open `pencil-new.pen` in [Pencil](https://pencil.evolus.vn/) to inspect.
