#!/usr/bin/env python3
"""Generates Tomato.ttf: a one glyph font with the tomato icon of the Pomodoro app at U+E000.

The launcher draws its icons as glyphs of jetbrains_mono_bold_20, so the tomato is merged into that font
(see fonts.json). The metrics match FontAwesome 5 (512 units per em, ascent 448, descent -64) so that the icon
has the same size and baseline as the other launcher icons.

The shape relies on the non-zero fill rule instead of boolean operations: the body is a clockwise oval, the
calyx is a counter-clockwise star inside it (a hole) and the stem is another clockwise contour that crosses the
hole and sticks out of the body.

Usage: python3 generate.py   (needs fontTools; writes ../Tomato.ttf)
"""

import math
import os

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

UNITS_PER_EM = 512
ASCENT = 448
DESCENT = -64
ADVANCE = 512
CODEPOINT = 0xE000

# Body: wide oval resting on the descent line
BODY_CENTER = (256, 160)
BODY_RADII = (240, 200)
# Calyx: flattened five pointed star near the top of the body
CALYX_CENTER = (256, 270)
CALYX_OUTER = (165, 80)
CALYX_INNER = (60, 30)
# Stem: slightly slanted bar from the centre of the calyx to the top of the em box
STEM_WIDTH = 64
STEM_BOTTOM = 270
STEM_TOP = ASCENT
STEM_SLANT = 40


def polygon(pen, points):
    pen.moveTo(points[0])
    for point in points[1:]:
        pen.lineTo(point)
    pen.closePath()


def oval(center, radii, steps=48, clockwise=True):
    sign = -1 if clockwise else 1
    return [(round(center[0] + radii[0] * math.cos(sign * 2 * math.pi * i / steps)),
             round(center[1] + radii[1] * math.sin(sign * 2 * math.pi * i / steps))) for i in range(steps)]


def star(center, outer, inner, tips=5, clockwise=False):
    sign = -1 if clockwise else 1
    points = []
    for i in range(tips * 2):
        angle = math.pi / 2 + sign * math.pi * i / tips
        radii = outer if i % 2 == 0 else inner
        points.append((round(center[0] + radii[0] * math.cos(angle)), round(center[1] + radii[1] * math.sin(angle))))
    return points


def tomato_glyph():
    pen = TTGlyphPen(None)
    polygon(pen, oval(BODY_CENTER, BODY_RADII, clockwise=True))
    polygon(pen, star(CALYX_CENTER, CALYX_OUTER, CALYX_INNER, clockwise=False))
    x = BODY_CENTER[0] - STEM_WIDTH // 2
    # clockwise in font coordinates (y up): left-bottom, left-top, right-top, right-bottom
    polygon(pen, [(x, STEM_BOTTOM), (x + STEM_SLANT, STEM_TOP), (x + STEM_SLANT + STEM_WIDTH, STEM_TOP),
                  (x + STEM_WIDTH, STEM_BOTTOM)])
    return pen.glyph()


def main():
    builder = FontBuilder(UNITS_PER_EM, isTTF=True)
    builder.setupGlyphOrder([".notdef", "tomato"])
    builder.setupCharacterMap({CODEPOINT: "tomato"})
    builder.setupGlyf({".notdef": TTGlyphPen(None).glyph(), "tomato": tomato_glyph()})
    builder.setupHorizontalMetrics({".notdef": (ADVANCE, 0), "tomato": (ADVANCE, 0)})
    builder.setupHorizontalHeader(ascent=ASCENT, descent=DESCENT)
    builder.setupNameTable({"familyName": "InfiniTime Tomato", "styleName": "Regular"})
    builder.setupOS2(sTypoAscender=ASCENT, sTypoDescender=DESCENT, usWinAscent=ASCENT, usWinDescent=-DESCENT)
    builder.setupPost()
    builder.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Tomato.ttf"))


if __name__ == "__main__":
    main()
