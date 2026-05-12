"""Fix PlatformIO ESP-IDF EMBED_FILES by creating stub .S files."""
import os, re

build_dir = ".pio/build/esp32-s3-devkitm-1"
ninja = os.path.join(build_dir, "build.ninja")
if not os.path.exists(ninja):
    print(f"No build.ninja yet, skipping")
    exit(0)

with open(ninja, encoding="utf-8", errors="replace") as f:
    content = f.read()

# Find ALL .S files needed as sources for .S.obj targets
# Pattern in ninja: build .../something.html.S.obj: ... C:/path/to/something.html.S ||
created = 0
for m in re.finditer(r'\|{2}\s*\n\s*build\s+\S+\.S\.obj:\s+\S+\s+(C:[^\s]+\.S)\s', content):
    s_path = m.group(1)
    # Normalize path
    s_path = s_path.replace("\\", "/")
    if not os.path.exists(s_path):
        os.makedirs(os.path.dirname(s_path), exist_ok=True)
        with open(s_path, "w") as f:
            f.write(".section .rodata.embedded\n.byte 0\n")
        print(f"  Created: {os.path.basename(s_path)}")
        created += 1

# Also check for the double-path pattern
for m in re.finditer(r'Source `([^`]+\.S)\'', content):
    s_path = m.group(1)
    if not os.path.exists(s_path):
        os.makedirs(os.path.dirname(s_path), exist_ok=True)
        with open(s_path, "w") as f:
            f.write(".section .rodata.embedded\n.byte 0\n")
        print(f"  Created (source): {os.path.basename(s_path)}")
        created += 1

if created:
    print(f"Fixed {created} embed files")
else:
    print("No embed files to fix")
