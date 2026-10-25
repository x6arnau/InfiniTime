#!/usr/bin/env python3
"""Generates Retro.ttf: the 4x5 pixel font of the Retro watch face.

Every glyph is drawn on a grid of square blocks, 5 rows tall (cap height) and usually 4 columns wide. One block is
100 font units and the em is 6 blocks (5 above the baseline, 1 below), so converting the font with
`lv_font_conv --size 6*b` gives blocks of exactly b pixels: the edges fall on the pixel grid and the glyphs stay
sharp at bpp 1. fonts.json converts it at several block sizes (retro_12, retro_18, retro_24, retro_54).

Digits share the same advance so that the time does not move when a digit changes. The other glyphs advance by
their width plus one block of spacing.

Usage: python3 generate.py   (needs fontTools; writes ../Retro.ttf)
"""

import os

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

BLOCK = 100
ROWS = 5
UNITS_PER_EM = 6 * BLOCK
ASCENT = ROWS * BLOCK
DESCENT = -BLOCK
DIGIT_ADVANCE = 5 * BLOCK
SPACE_ADVANCE = 3 * BLOCK

# Rows from top to bottom, '#' is a filled block. All rows of a glyph have the same width.
GLYPHS = {
    "0": [".##.", "#..#", "#..#", "#..#", ".##."],
    "1": ["..#.", ".##.", "..#.", "..#.", ".###"],
    "2": ["###.", "...#", ".##.", "#...", "####"],
    "3": ["###.", "...#", ".##.", "...#", "###."],
    "4": ["#.#.", "#.#.", "####", "..#.", "..#."],
    "5": ["####", "#...", "###.", "...#", "###."],
    "6": [".##.", "#...", "###.", "#..#", ".##."],
    "7": ["####", "...#", "..#.", ".#..", ".#.."],
    "8": [".##.", "#..#", ".##.", "#..#", ".##."],
    "9": [".##.", "#..#", ".###", "...#", ".##."],
    "A": [".##.", "#..#", "####", "#..#", "#..#"],
    "B": ["###.", "#..#", "###.", "#..#", "###."],
    "C": [".###", "#...", "#...", "#...", ".###"],
    "D": ["###.", "#..#", "#..#", "#..#", "###."],
    "E": ["###", "#..", "###", "#..", "###"],
    "F": ["###", "#..", "###", "#..", "#.."],
    "G": [".###", "#...", "#.##", "#..#", ".###"],
    "H": ["#..#", "#..#", "####", "#..#", "#..#"],
    "I": ["###", ".#.", ".#.", ".#.", "###"],
    "J": ["...#", "...#", "...#", "#..#", ".##."],
    "K": ["#..#", "#.#.", "##..", "#.#.", "#..#"],
    "L": ["#..", "#..", "#..", "#..", "###"],
    "M": ["#...#", "##.##", "#.#.#", "#...#", "#...#"],
    "N": ["#..#", "##.#", "#.##", "#..#", "#..#"],
    "O": [".##.", "#..#", "#..#", "#..#", ".##."],
    "P": ["###.", "#..#", "###.", "#...", "#..."],
    "Q": [".##.", "#..#", "#..#", "#.#.", ".#.#"],
    "R": ["###.", "#..#", "###.", "#.#.", "#..#"],
    "S": [".###", "#...", ".##.", "...#", "###."],
    "T": ["###", ".#.", ".#.", ".#.", ".#."],
    "U": ["#..#", "#..#", "#..#", "#..#", ".##."],
    "V": ["#.#", "#.#", "#.#", "#.#", ".#."],
    "W": ["#...#", "#...#", "#.#.#", "##.##", "#...#"],
    "X": ["#.#", "#.#", ".#.", "#.#", "#.#"],
    "Y": ["#.#", "#.#", ".#.", ".#.", ".#."],
    "Z": ["####", "...#", ".##.", "#...", "####"],
    ":": [".", "#", ".", "#", "."],
    ".": [".", ".", ".", ".", "#"],
    "|": ["#", "#", "#", "#", "#"],
    "-": ["...", "...", "###", "...", "..."],
    "%": ["##..#", "##.#.", "..#..", ".#.##", "#..##"],
    "°": ["###", "#.#", "###", "...", "..."],
}


def glyph_name(char):
    return "uni%04X" % ord(char)


def draw(rows):
    """One clockwise rectangle per horizontal run of blocks."""
    pen = TTGlyphPen(None)
    for row_index, row in enumerate(rows):
        bottom = (ROWS - 1 - row_index) * BLOCK
        top = bottom + BLOCK
        column = 0
        while column < len(row):
            if row[column] != "#":
                column += 1
                continue
            start = column
            while column < len(row) and row[column] == "#":
                column += 1
            left, right = start * BLOCK, column * BLOCK
            pen.moveTo((left, bottom))
            pen.lineTo((left, top))
            pen.lineTo((right, top))
            pen.lineTo((right, bottom))
            pen.closePath()
    return pen.glyph()


def main():
    for char, rows in GLYPHS.items():
        assert len(rows) == ROWS and len({len(row) for row in rows}) == 1, char

    glyphs = {".notdef": TTGlyphPen(None).glyph(), "space": TTGlyphPen(None).glyph()}
    metrics = {".notdef": (SPACE_ADVANCE, 0), "space": (SPACE_ADVANCE, 0)}
    cmap = {ord(" "): "space"}
    for char, rows in GLYPHS.items():
        name = glyph_name(char)
        glyphs[name] = draw(rows)
        width = len(rows[0]) * BLOCK
        metrics[name] = (DIGIT_ADVANCE if char.isdigit() else width + BLOCK, 0)
        cmap[ord(char)] = name

    builder = FontBuilder(UNITS_PER_EM, isTTF=True)
    builder.setupGlyphOrder(list(glyphs))
    builder.setupCharacterMap(cmap)
    builder.setupGlyf(glyphs)
    builder.setupHorizontalMetrics(metrics)
    builder.setupHorizontalHeader(ascent=ASCENT, descent=DESCENT)
    builder.setupNameTable({"familyName": "InfiniTime Retro", "styleName": "Regular"})
    builder.setupOS2(sTypoAscender=ASCENT, sTypoDescender=DESCENT, usWinAscent=ASCENT, usWinDescent=-DESCENT)
    builder.setupPost()
    builder.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Retro.ttf"))


if __name__ == "__main__":
    main()
