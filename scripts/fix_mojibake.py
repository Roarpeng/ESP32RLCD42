#!/usr/bin/env python3
"""Fix double-encoded UTF-8 mojibake on a few specific WiFi QR page lines."""
import re

path = 'src/boards/waveshare-s3-rlcd-4.2/custom_lcd_display.cc'
data = open(path, 'rb').read()

# 替换映射: (双编码 mojibake bytes) -> (proper UTF-8 bytes)
# 双编码: 原 utf-8 字节被当作 latin-1 再编 utf-8。反过来还原即可。
def make_pair(zh_text):
    # 当前文件中的字节 = zh_text.encode('utf-8').decode('latin-1').encode('utf-8')
    bad = zh_text.encode('utf-8').decode('latin-1').encode('utf-8')
    good = zh_text.encode('utf-8')
    return bad, good

pairs = [
    make_pair('热点: '),
    make_pair('配网页: '),
    make_pair('二维码内存不足'),
]

count = 0
for bad, good in pairs:
    if bad in data:
        c = data.count(bad)
        data = data.replace(bad, good)
        count += c
        print(f'replaced {c} occurrence(s) of {good.decode("utf-8")!r}')

open(path, 'wb').write(data)
print(f'total {count} replacements')
