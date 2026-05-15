"""Pencil page builder.

Reads per-page layout specs from ``scripts.pencil_specs.*`` (one Python module
per page, each exposing a ``SPEC`` dict), keeps the reusable components from
the previous ``pencil-new.pen`` intact, and produces a fresh ``pencil-new.pen``
that reflects the actual page-rendering source code under
``src/boards/waveshare-s3-rlcd-4.2/``.

Each page spec uses the small DSL below.  The builder lowers it to the verbose
Pencil JSON schema (version 2.11) automatically.

DSL element types (every element accepts ``x``, ``y``, ``w``, ``h`` and an
optional ``name`` — coordinates are page-local pixels):

* ``ref``:        ``{type: "ref", ref_id, x, y, w, h, name}``
                  ref_id ∈ {HVJtj=AI bar, UaGTC=mini status,
                            b5t5X=7-seg digit, bd3Wx=colon dots,
                            xDTeq=corner deco}.
* ``rect``:       Solid-fill rectangle. Default fill ``#000000``.
                  Optional: ``fill``, ``opacity``, ``radius``.
* ``hollow``:     Stroked rectangle. Optional: ``stroke`` (default 1),
                  ``radius``, ``fill`` (default white), ``opacity``.
* ``line``:       Horizontal/vertical line (alias for rect with ``opacity=0.15``
                  unless overridden).
* ``label``:      Text. Required: ``text``.  Optional:
                  ``size`` (font px, default 14), ``align``
                  (left/center/right, default left),
                  ``weight`` (normal/bold, default normal),
                  ``opacity`` (default 0.8), ``font`` (default Inter),
                  ``letter_spacing``.
* ``ellipse``:    Filled ellipse. Optional: ``fill``, ``opacity``.
* ``icon``:       Lucide icon. Required: ``icon`` (icon name).
                  Optional: ``fill``, ``family`` (default lucide).
* ``frame``:      Container.  Optional: ``fill`` (default white),
                  ``stroke`` ({thickness, fill, align}), ``radius``,
                  ``children`` (list of more DSL elements), ``opacity``,
                  ``padding``, ``gap``, ``layout``, ``justify``, ``align_items``.

Run with ``python3 scripts/build_pencil.py``.
"""

from __future__ import annotations

import importlib
import json
import os
import string
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PEN_PATH = ROOT / "pencil-new.pen"
SPECS_DIR = ROOT / "scripts" / "pencil_specs"

sys.path.insert(0, str(ROOT))

REUSABLE_IDS = ["xDTeq", "HVJtj", "UaGTC", "b5t5X", "bd3Wx"]


def short_id(seed: int, length: int = 5) -> str:
    alphabet = string.ascii_letters + string.digits
    out = []
    for _ in range(length):
        out.append(alphabet[seed % len(alphabet)])
        seed = (seed * 31 + 7) % (1 << 31)
    return "".join(out)


_PAGE_PREFIX = {
    "page_clock":     "cl",
    "page_weather":   "we",
    "page_quote":     "qu",
    "page_photo":     "ph",
    "page_pomodoro":  "po",
    "page_music":     "mu",
    "page_wifi_qr":   "qr",
    "page_settings":  "se",
}


class IdAllocator:
    def __init__(self, page_id: str):
        self._prefix = _PAGE_PREFIX.get(page_id, page_id[:2])
        self._counter = 0

    def next(self) -> str:
        self._counter += 1
        return f"{self._prefix}{self._counter:04d}"


def to_color(value, default: str = "#000000") -> str:
    if value is None:
        return default
    return value


def build_text(el: dict, alloc: IdAllocator) -> dict:
    out = {
        "type": "text",
        "id": el.get("id") or alloc.next(),
        "x": el.get("x", 0),
        "y": el.get("y", 0),
        "name": el.get("name", "label"),
        "opacity": el.get("opacity", 0.8),
        "fill": to_color(el.get("fill")),
        "content": el["text"],
        "fontFamily": el.get("font", "Inter"),
        "fontSize": el.get("size", 14),
        "fontWeight": el.get("weight", "normal"),
    }
    if "align" in el:
        out["textAlign"] = el["align"]
    if "w" in el:
        out["width"] = el["w"]
        out["textGrowth"] = "fixed-width"
    if "h" in el:
        out["height"] = el["h"]
    if "letter_spacing" in el:
        out["letterSpacing"] = el["letter_spacing"]
    return out


def build_rect(el: dict, alloc: IdAllocator, default_opacity=1.0) -> dict:
    out = {
        "type": "frame",
        "id": el.get("id") or alloc.next(),
        "x": el.get("x", 0),
        "y": el.get("y", 0),
        "name": el.get("name", "rect"),
        "width": el.get("w", 1),
        "height": el.get("h", 1),
        "fill": to_color(el.get("fill")),
    }
    op = el.get("opacity", default_opacity)
    if op != 1.0:
        out["opacity"] = op
    if "radius" in el:
        out["cornerRadius"] = el["radius"]
    return out


def build_line(el: dict, alloc: IdAllocator) -> dict:
    rect_el = dict(el)
    rect_el.setdefault("opacity", 0.15)
    rect_el.setdefault("h", 1)
    return build_rect(rect_el, alloc, default_opacity=0.15)


def build_hollow(el: dict, alloc: IdAllocator) -> dict:
    out = {
        "type": "frame",
        "id": el.get("id") or alloc.next(),
        "x": el.get("x", 0),
        "y": el.get("y", 0),
        "name": el.get("name", "hollow"),
        "width": el.get("w", 1),
        "height": el.get("h", 1),
        "fill": to_color(el.get("fill"), "#FFFFFF"),
        "stroke": {
            "align": el.get("stroke_align", "inside"),
            "thickness": el.get("stroke", 1),
            "fill": to_color(el.get("stroke_color"), "#000000"),
        },
    }
    if "radius" in el:
        out["cornerRadius"] = el["radius"]
    if "opacity" in el:
        out["opacity"] = el["opacity"]
    if el.get("stroke_dashed"):
        out["stroke"]["dashArray"] = el.get("dash_array", [6, 4])
    return out


def build_ellipse(el: dict, alloc: IdAllocator) -> dict:
    out = {
        "type": "ellipse",
        "id": el.get("id") or alloc.next(),
        "x": el.get("x", 0),
        "y": el.get("y", 0),
        "name": el.get("name", "ellipse"),
        "fill": to_color(el.get("fill")),
        "width": el.get("w", 8),
        "height": el.get("h", 8),
    }
    if "opacity" in el:
        out["opacity"] = el["opacity"]
    if el.get("stroke"):
        out["stroke"] = {
            "align": el.get("stroke_align", "inside"),
            "thickness": el["stroke"],
            "fill": to_color(el.get("stroke_color"), "#000000"),
        }
        out["fill"] = to_color(el.get("fill"), "#FFFFFF")
    return out


def build_icon(el: dict, alloc: IdAllocator) -> dict:
    out = {
        "type": "icon_font",
        "id": el.get("id") or alloc.next(),
        "x": el.get("x", 0),
        "y": el.get("y", 0),
        "name": el.get("name", "icon"),
        "width": el.get("w", 16),
        "height": el.get("h", 16),
        "iconFontName": el["icon"],
        "iconFontFamily": el.get("family", "lucide"),
        "fill": to_color(el.get("fill")),
    }
    if "opacity" in el:
        out["opacity"] = el["opacity"]
    return out


def build_ref(el: dict, alloc: IdAllocator) -> dict:
    out = {
        "id": el.get("id") or alloc.next(),
        "type": "ref",
        "ref": el["ref_id"],
        "x": el.get("x", 0),
        "y": el.get("y", 0),
        "name": el.get("name", "ref"),
        "flipX": False,
        "flipY": False,
    }
    if "w" in el:
        out["width"] = el["w"]
    if "h" in el:
        out["height"] = el["h"]
    return out


def build_frame(el: dict, alloc: IdAllocator) -> dict:
    out = {
        "type": "frame",
        "id": el.get("id") or alloc.next(),
        "x": el.get("x", 0),
        "y": el.get("y", 0),
        "name": el.get("name", "frame"),
        "width": el.get("w", 1),
        "height": el.get("h", 1),
    }
    if "fill" in el:
        out["fill"] = to_color(el.get("fill"))
    if "radius" in el:
        out["cornerRadius"] = el["radius"]
    if "opacity" in el:
        out["opacity"] = el["opacity"]
    if "stroke" in el:
        s = el["stroke"]
        if isinstance(s, dict):
            stroke = {
                "align": s.get("align", "inside"),
                "thickness": s.get("thickness", 1),
                "fill": to_color(s.get("fill"), "#000000"),
            }
            if s.get("dashed"):
                stroke["dashArray"] = s.get("dash_array", [6, 4])
            out["stroke"] = stroke
        else:
            out["stroke"] = {"align": "inside", "thickness": s, "fill": "#000000"}
    if "padding" in el:
        out["padding"] = el["padding"]
    if "gap" in el:
        out["gap"] = el["gap"]
    if "layout" in el:
        out["layout"] = el["layout"]
    if "justify" in el:
        out["justifyContent"] = el["justify"]
    if "align_items" in el:
        out["alignItems"] = el["align_items"]
    if "children" in el:
        out["children"] = [build_element(child, alloc) for child in el["children"]]
    return out


BUILDERS = {
    "rect": build_rect,
    "line": build_line,
    "hollow": build_hollow,
    "label": build_text,
    "text": build_text,
    "ellipse": build_ellipse,
    "icon": build_icon,
    "ref": build_ref,
    "frame": build_frame,
}


def build_element(el: dict, alloc: IdAllocator) -> dict:
    t = el["type"]
    if t not in BUILDERS:
        raise ValueError(f"Unknown element type: {t}")
    return BUILDERS[t](el, alloc)


def build_page_frame(spec: dict, x_offset: int) -> dict:
    alloc = IdAllocator(spec["page_id"])
    frame = {
        "type": "frame",
        "id": spec["page_id"],
        "x": x_offset,
        "y": 89,
        "name": spec["name"],
        "reusable": None,
        "width": spec.get("width", 400),
        "height": spec.get("height", 300),
        "layout": "none",
        "children": [build_element(el, alloc) for el in spec["elements"]],
    }
    return frame


def load_specs():
    specs = []
    for p in sorted(SPECS_DIR.glob("*.py")):
        if p.name.startswith("_"):
            continue
        mod_name = f"scripts.pencil_specs.{p.stem}"
        try:
            mod = importlib.import_module(mod_name)
        except Exception as e:
            print(f"  ! failed to import {mod_name}: {e}", file=sys.stderr)
            raise
        if not hasattr(mod, "SPEC"):
            print(f"  ! skipping {mod_name}: no SPEC attribute", file=sys.stderr)
            continue
        specs.append((p.stem, mod.SPEC))
    return specs


def load_reusables():
    with open(PEN_PATH, "r") as f:
        data = json.load(f)
    reusable = [c for c in data["children"] if c.get("id") in REUSABLE_IDS]
    return reusable


def main():
    print("Loading reusable components from existing .pen file …")
    reusables = load_reusables()
    print(f"  → kept {len(reusables)} reusable components: "
          f"{[r['name'] for r in reusables]}")

    print(f"Loading per-page specs from {SPECS_DIR} …")
    specs = load_specs()
    print(f"  → loaded {len(specs)} page specs: "
          f"{[name for name, _ in specs]}")

    children = list(reusables)
    x = -7
    for stem, spec in specs:
        page = build_page_frame(spec, x_offset=x)
        children.append(page)
        x += 420
        n_el = len(spec.get("elements", []))
        print(f"  → page '{spec['name']}': {n_el} top-level elements")

    out = {"version": "2.11", "children": children}
    with open(PEN_PATH, "w") as f:
        json.dump(out, f, indent=2, ensure_ascii=False)
    print(f"\nWrote {PEN_PATH} ({os.path.getsize(PEN_PATH):,} bytes)")


if __name__ == "__main__":
    main()
