"""Spec for legacy music page, derived from
music_ui.cc :: SetupMusicUI().
"""

SPEC = {
    "name": "6. Music Player",
    "page_id": "page_music",
    "width": 400,
    "height": 300,
    "elements": [
        # === Header: AI bar (0,0,220x32) + double 1-px seps + mini status ===
        # BuildAiBar(page, 0, 0, 220, bar_index=5 MUSIC, dark=true).  The
        # firmware paints the music page on a black background, but the
        # Pencil mockup is the standard white-on-black design surface, so
        # we render the AI bar and status row using the same refs as the
        # other pages.
        {"type": "ref", "ref_id": "HVJtj", "x": 0, "y": 0, "w": 220, "h": 32,
         "name": "aiBar"},
        # DesktopHeaderSepDouble: two 1-px separators at y=32 and y=34.
        {"type": "rect", "x": 0, "y": 32, "w": 400, "h": 1,
         "opacity": 0.15, "name": "headerSepTop"},
        {"type": "rect", "x": 0, "y": 34, "w": 400, "h": 1,
         "opacity": 0.15, "name": "headerSepBottom"},
        # Mini status bar (224, 4, 175x28) — same UaGTC ref as other pages.
        {"type": "ref", "ref_id": "UaGTC", "x": 224, "y": 4, "w": 175,
         "name": "miniStatus"},

        # === Vinyl card: hollow 1-px black frame at (8, 44, 140, 140) ===
        {"type": "hollow", "x": 8, "y": 44, "w": 140, "h": 140,
         "stroke": 1, "stroke_color": "#000000", "radius": 0,
         "name": "vinylCard"},
        # 4 concentric circles centred at (78, 114) absolute — i.e. (70, 70)
        # local to the vinyl card.  Radii 52 / 42 / 32 / 20 (diameters
        # 104 / 84 / 64 / 40), hollow with 1-px black stroke.
        {"type": "ellipse", "x": 26, "y": 62, "w": 104, "h": 104,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "vinylRing1"},
        {"type": "ellipse", "x": 36, "y": 72, "w": 84, "h": 84,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "vinylRing2"},
        {"type": "ellipse", "x": 46, "y": 82, "w": 64, "h": 64,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "vinylRing3"},
        {"type": "ellipse", "x": 58, "y": 94, "w": 40, "h": 40,
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "vinylCenterLabel"},
        # Small 10x10 solid-black centre dot (axis hole).
        {"type": "ellipse", "x": 73, "y": 109, "w": 10, "h": 10,
         "fill": "#000000", "name": "vinylHole"},

        # === Song card: hollow 1-px black frame at (160, 44, 232, 140) ===
        {"type": "hollow", "x": 160, "y": 44, "w": 232, "h": 140,
         "stroke": 1, "stroke_color": "#000000", "radius": 0,
         "name": "songCard"},
        # Song title "未播放" near the top (size 16, bold).
        {"type": "label", "x": 170, "y": 54, "w": 212, "text": "未播放",
         "size": 16, "weight": "bold", "name": "songTitle"},
        # Artist "未知歌手" below the title (size 14, opacity 0.55).
        {"type": "label", "x": 170, "y": 76, "w": 212, "text": "未知歌手",
         "size": 14, "opacity": 0.55, "name": "songArtist"},
        # 1-px horizontal separator across the card.
        {"type": "line", "x": 170, "y": 100, "w": 212, "name": "songSep"},
        # Three stacked lyric lines, all centred horizontally in the card.
        {"type": "label", "x": 160, "y": 112, "w": 232, "text": "",
         "size": 14, "align": "center", "opacity": 0.4,
         "name": "lyricPrev"},
        {"type": "label", "x": 160, "y": 134, "w": 232,
         "text": "等待播放...",
         "size": 16, "weight": "bold", "align": "center",
         "name": "lyricCurrent"},
        {"type": "label", "x": 160, "y": 158, "w": 232, "text": "",
         "size": 14, "align": "center", "opacity": 0.4,
         "name": "lyricNext"},

        # === Progress bar at (12, 198, 280, 12) ===
        # Outlined track (1-px black border, white fill).
        {"type": "hollow", "x": 12, "y": 198, "w": 280, "h": 12,
         "stroke": 1, "stroke_color": "#000000", "radius": 0,
         "name": "progressTrack"},
        # Filled portion (solid black) inset 1 px inside the track.
        {"type": "rect", "x": 13, "y": 199, "w": 50, "h": 10,
         "fill": "#000000", "name": "progressFill"},

        # === Timing label "00:00 / 00:00" at (296, 196), size 14 ===
        {"type": "label", "x": 296, "y": 196, "text": "00:00 / 00:00",
         "size": 14, "name": "progressTime"},

        # === Page-indicator dots (BuildPageDots is called for MODE_MUSIC, but
        # the music page is not part of the 5-desktop cycle, so we render all
        # five dots hollow with no active dot.) ===
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
         "fill": "#FFFFFF", "stroke": 1, "stroke_color": "#000000",
         "name": "pageDot5"},
    ],
}
