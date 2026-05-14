#!/usr/bin/env python3
"""Convert emoji RGB565A8 images to 1-bit (I1) for monochrome RLCD display."""

import re, os, sys

SRC_DIR = os.path.join(os.path.dirname(__file__), '..', 'managed_components', '78__xiaozhi-fonts', 'src', 'emoji')

def convert_file(path):
    with open(path) as f:
        src = f.read()

    m_w = re.search(r'\.header\.w\s*=\s*(\d+)', src)
    m_h = re.search(r'\.header\.h\s*=\s*(\d+)', src)
    if not m_w or not m_h:
        return False
    w, h = int(m_w.group(1)), int(m_h.group(1))

    m_name = re.search(r'const\s+lv_image_dsc_t\s+(\w+)\s*=', src)
    if not m_name:
        return False
    symbol_name = m_name.group(1)
    map_name = symbol_name + '_map'

    m_data = re.search(r'uint8_t\s+\w+_map\[\]\s*=\s*\{([^}]+)\}', src, re.DOTALL)
    if not m_data:
        return False
    hex_vals = re.findall(r'0x([0-9a-fA-F]{2})', m_data.group(1))
    raw = bytes(int(h, 16) for h in hex_vals)

    rgb_size = w * 2 * h
    alpha_offset = rgb_size
    row_bytes = (w + 7) // 8

    palette = bytes([0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF])
    bits = bytearray(row_bytes * h)

    for y in range(h):
        for x in range(w):
            px_off = (y * w + x) * 2
            alpha_off = alpha_offset + y * w + x

            if px_off + 1 < len(raw) and alpha_off < len(raw):
                lo, hi = raw[px_off], raw[px_off + 1]
                pixel = (hi << 8) | lo
                r = ((pixel >> 11) & 0x1F) * 255 // 31
                g = ((pixel >> 5) & 0x3F) * 255 // 63
                b = (pixel & 0x1F) * 255 // 31
                gray = int(0.299 * r + 0.587 * g + 0.114 * b)
                alpha = raw[alpha_off]
                gray = gray * alpha // 255
            else:
                gray = 0

            if gray > 127:
                byte_idx = y * row_bytes + x // 8
                bits[byte_idx] |= (0x80 >> (x & 7))

    new_data = palette + bytes(bits)

    hex_lines = []
    for i in range(0, len(new_data), 16):
        chunk = new_data[i:i+16]
        hex_lines.append('    ' + ','.join(f'0x{b:02x}' for b in chunk) + ',')

    attr_upper = symbol_name.upper().replace('EMOJI_', 'EMOJI_')
    new_src = f'''
#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#elif defined(LV_BUILD_TEST)
#include "../lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_{attr_upper}
#define LV_ATTRIBUTE_{attr_upper}
#endif

static const
LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_{attr_upper}
uint8_t {map_name}[] = {{

{chr(10).join(hex_lines)}

}};

const lv_image_dsc_t {symbol_name} = {{
  .header.magic = LV_IMAGE_HEADER_MAGIC,
  .header.cf = LV_COLOR_FORMAT_I1,
  .header.flags = 0,
  .header.w = {w},
  .header.h = {h},
  .header.stride = {row_bytes},
  .data_size = sizeof({map_name}),
  .data = {map_name},
}};
'''

    with open(path, 'w') as f:
        f.write(new_src)
    return True


def main():
    src_dir = os.path.abspath(SRC_DIR)
    files = sorted(f for f in os.listdir(src_dir) if f.endswith('.c'))
    converted = 0
    for fname in files:
        path = os.path.join(src_dir, fname)
        if convert_file(path):
            converted += 1
            print(f'  Converted: {fname}')
        else:
            print(f'  Skipped: {fname}')
    print(f'\nDone: {converted}/{len(files)} files converted to 1-bit I1 format')


if __name__ == '__main__':
    main()
