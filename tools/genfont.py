#!/usr/bin/env python3
"""Генерирует core/fonts.cpp: сглаженные шрифты 4 бита на пиксель (латиница, кириллица, символы)."""
import sys
from PIL import ImageFont

FONTS = [
    # имя, файл, размер
    ("font_small", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 11),
    ("font_ui", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 13),
    ("font_big", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 20),
    ("font_mono", "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 9),
]

CHARS = [chr(c) for c in range(0x20, 0x7F)]
CHARS += [chr(c) for c in range(0x410, 0x450)] + ["Ё", "ё", "°", "•", "…", "—", "«", "»", "×", "→", "←", "↓", "–", "✓", "✗", "▶", "·"]


def gen(name, path, size, out):
    f = ImageFont.truetype(path, size)
    ascent, descent = f.getmetrics()
    glyphs = []
    bits = bytearray()
    for ch in sorted(set(CHARS), key=ord):
        adv = round(f.getlength(ch))
        box = f.getbbox(ch)
        if ch == " " or box[2] <= box[0]:
            glyphs.append((ord(ch), 0, 0, 0, 0, adv, len(bits)))
            continue
        mask = f.getmask(ch, mode="L")
        w, h = mask.size
        # getmask обрезает по bbox: смещение = левый/верхний край bbox
        x0, y0 = box[0], box[1]
        off = len(bits)
        for y in range(h):
            row = [mask.getpixel((x, y)) for x in range(w)]
            if w % 2:
                row.append(0)
            for i in range(0, len(row), 2):
                a = (row[i] + 8) // 17
                b = (row[i + 1] + 8) // 17
                bits.append((min(a, 15) << 4) | min(b, 15))
        glyphs.append((ord(ch), w, h, x0, y0, adv, off))
    out.append(f"static const uint8_t {name}_bits[] = {{")
    for i in range(0, len(bits), 24):
        out.append("  " + ",".join(str(b) for b in bits[i:i + 24]) + ",")
    out.append("};")
    out.append(f"static const Glyph {name}_glyphs[] = {{")
    for g in glyphs:
        out.append("  {%d,%d,%d,%d,%d,%d,%d}," % g)
    out.append("};")
    out.append(f"const Font {name} = {{ {name}_glyphs, {name}_bits, {len(glyphs)}, {ascent + descent}, {ascent} }};")
    out.append("")


def main():
    out = ["// Сгенерировано tools/genfont.py из шрифтов DejaVu (свободная лицензия Bitstream Vera). Не править.",
           '#include "gfx.h"', 'namespace gfx {', ""]
    for n, p, s in FONTS:
        gen(n, p, s, out)
    out.append("}  // namespace gfx")
    open(sys.argv[1], "w").write("\n".join(out))


main()
