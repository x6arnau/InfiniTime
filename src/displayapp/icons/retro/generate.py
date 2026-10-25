#!/usr/bin/env python3
"""Generates retro_icons.c: the pixel art icons of the Retro watch face.

Each icon is drawn below as rows of characters, one character per block and one colour per character. The images
are stored already scaled to their final size and as indexed images with the smallest palette that fits
(1, 2 or 4 bits per pixel), so LVGL draws them without any transformation and they take little flash.

Like the other icons, the file is included by the screen that uses it (WatchFaceRetro.cpp).

Usage: python3 generate.py   (writes retro_icons.c next to this script)
"""

import os

# Character -> RGB. Index 0 of every palette is the background ('.').
COLORS = {
    ".": (0x00, 0x00, 0x00),
    "W": (0xFF, 0xFF, 0xFF),
    "g": (0x90, 0x90, 0x90),
    "G": (0x5A, 0xD8, 0x2E),
    "R": (0xE8, 0x30, 0x30),
    "Y": (0xF8, 0xC8, 0x20),
    "B": (0x40, 0x80, 0xF8),
}

# name -> (block size in pixels, rows)
ICONS = {
    "retro_shoe": (3, [
        "..GGG.........",
        ".GGGGG........",
        ".GGGGGG.......",
        ".GGGGGGG......",
        ".GGGGWGGGG....",
        ".GGGGGGGGGGG..",
        "..GGGGGGGGGGGG",
        "..WWWWWWWWWWWW",
        "..WWWWWWWWWWW.",
    ]),
    "retro_heart": (5, [
        ".RR.RR.",
        "RRRRRRR",
        "RRRRRRR",
        ".RRRRR.",
        "..RRR..",
        "...R...",
    ]),
    "retro_battery": (2, [
        "WWWWWWWWWWWWWWWW.",
        "W..............W.",
        "W..............W.",
        "W..............WW",
        "W..............WW",
        "W..............WW",
        "W..............WW",
        "W..............W.",
        "W..............W.",
        "WWWWWWWWWWWWWWWW.",
    ]),
    "retro_sun": (3, [
        ".......Y.......",
        "..Y....Y....Y..",
        "...Y.......Y...",
        ".....YYYYY.....",
        "....YYYYYYY....",
        "YY..YYYYYYY..YY",
        "....YYYYYYY....",
        ".....YYYYY.....",
        "...Y.......Y...",
        "..Y....Y....Y..",
        ".......Y.......",
        "...............",
    ]),
    "retro_moon": (3, [
        "...............",
        ".....YYYY......",
        "...YYYY........",
        "..YYY..........",
        ".YYY...........",
        ".YYY...........",
        ".YYY...........",
        ".YYYY.......Y..",
        "..YYYYY..YYYY..",
        "...YYYYYYYYY...",
        ".....YYYYY.....",
        "...............",
    ]),
    "retro_clouds_sun": (3, [
        "......Y........",
        "......Y........",
        "..Y.......Y....",
        "....YYYYY......",
        "...YYYYYYY.....",
        "YY.YYYYYYY.YY..",
        "...YYYYYWWWW...",
        "....YYYWWWWWW..",
        "..Y..WWWWWWWWW.",
        "....WWWWWWWWWWW",
        "....WWWWWWWWWWW",
        ".....ggggggggg.",
    ]),
    "retro_clouds_moon": (3, [
        "...............",
        "...YYY.........",
        "..YY...........",
        ".YY............",
        ".YY....WWW.....",
        ".YY...WWWWW....",
        "..YYWWWWWWWW...",
        "...WWWWWWWWWWW.",
        "..WWWWWWWWWWWWW",
        "..WWWWWWWWWWWWW",
        "...ggggggggggg.",
        "...............",
    ]),
    "retro_clouds": (3, [
        "...............",
        "...............",
        "......WWW......",
        ".....WWWWW.....",
        "...WWWWWWWW....",
        "..WWWWWWWWWWW..",
        ".WWWWWWWWWWWWW.",
        ".WWWWWWWWWWWWW.",
        "..ggggggggggg..",
        "...............",
        "...............",
        "...............",
    ]),
    "retro_broken_clouds": (3, [
        "...............",
        "........ggg....",
        ".......ggggg...",
        "....WWW.ggggg..",
        "...WWWWWgggggg.",
        ".WWWWWWWW.gggg.",
        "WWWWWWWWWWW....",
        "WWWWWWWWWWW....",
        ".ggggggggg.....",
        "...............",
        "...............",
        "...............",
    ]),
    "retro_shower": (3, [
        "...............",
        "......WWW......",
        ".....WWWWW.....",
        "...WWWWWWWW....",
        "..WWWWWWWWWWW..",
        ".WWWWWWWWWWWWW.",
        "..ggggggggggg..",
        "...............",
        "...B..B..B..B..",
        "..B..B..B..B...",
        "...............",
        ".B..B..B..B....",
    ]),
    "retro_sun_rain": (3, [
        "...Y...........",
        "Y.....Y........",
        "..YYYY.........",
        ".YYYYYWWW......",
        ".YYYYWWWWW.....",
        "..YWWWWWWWWW...",
        "..WWWWWWWWWWWW.",
        ".WWWWWWWWWWWWWW",
        "...............",
        "...B...B...B...",
        "..B...B...B....",
        "...............",
    ]),
    "retro_thunder": (3, [
        "...............",
        "......ggg......",
        ".....ggggg.....",
        "...gggggggg....",
        "..ggggggggggg..",
        ".ggggggggggggg.",
        ".ggggYYggggggg.",
        ".....YY........",
        "....YYYYY......",
        "......YY.......",
        ".....YY........",
        "....Y..........",
    ]),
    "retro_snow": (3, [
        "...............",
        "......WWW......",
        ".....WWWWW.....",
        "...WWWWWWWW....",
        "..WWWWWWWWWWW..",
        ".WWWWWWWWWWWWW.",
        "..ggggggggggg..",
        "...............",
        "..W...W...W....",
        ".WWW.WWW.WWW...",
        "..W...W...W....",
        "...............",
    ]),
    "retro_mist": (3, [
        "...............",
        "...............",
        "...WWWWWWWWW...",
        "...............",
        ".ggggggg.ggggg.",
        "...............",
        "..WWWW.WWWWWWW.",
        "...............",
        ".ggggggggggg...",
        "...............",
        "....WWWWWW.....",
        "...............",
    ]),
    "retro_unknown": (3, [
        "...............",
        "...............",
        "......ggg......",
        ".....ggggg.....",
        "...gggggggg....",
        "..ggggggggggg..",
        ".ggggggggggggg.",
        ".ggggggggggggg.",
        "...............",
        "...............",
        "...............",
        "...............",
    ]),
}

FORMATS = {1: "LV_IMG_CF_INDEXED_1BIT", 2: "LV_IMG_CF_INDEXED_2BIT", 4: "LV_IMG_CF_INDEXED_4BIT"}


def convert(name, scale, rows):
    assert len({len(row) for row in rows}) == 1, name
    palette = ["."] + sorted({char for row in rows for char in row} - {"."})
    bpp = next(bits for bits in (1, 2, 4) if len(palette) <= 1 << bits)
    width, height = len(rows[0]) * scale, len(rows) * scale

    lines = [f"const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t {name}_map[] = {{"]
    for index in range(1 << bpp):
        red, green, blue = COLORS[palette[index]] if index < len(palette) else (0, 0, 0)
        lines.append(f"  0x{blue:02x}, 0x{green:02x}, 0x{red:02x}, 0xff,")
    data_size = (1 << bpp) * 4
    for row in rows:
        pixels = [palette.index(char) for char in row for _ in range(scale)]
        packed = []
        for start in range(0, width, 8 // bpp):
            byte = 0
            for offset, pixel in enumerate(pixels[start:start + 8 // bpp]):
                byte |= pixel << (8 - bpp * (offset + 1))
            packed.append(byte)
        line = "  " + " ".join(f"0x{byte:02x}," for byte in packed)
        lines.extend([line] * scale)
        data_size += len(packed) * scale
    lines.append("};")
    lines.append("")
    lines.append(f"const lv_img_dsc_t {name} = {{")
    lines.append(f"  {{{FORMATS[bpp]}, 0, 0, {width}, {height}}},")
    lines.append(f"  {data_size},")
    lines.append(f"  {name}_map,")
    lines.append("};")
    return "\n".join(lines)


def main():
    source = ["// Generated by generate.py, do not edit", "#include <lvgl/lvgl.h>", "",
              "#ifndef LV_ATTRIBUTE_MEM_ALIGN", "#define LV_ATTRIBUTE_MEM_ALIGN", "#endif", ""]
    source += [convert(name, scale, rows) + "\n" for name, (scale, rows) in ICONS.items()]
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "retro_icons.c"), "w", newline="\n") as file:
        file.write("\n".join(source))


if __name__ == "__main__":
    main()
