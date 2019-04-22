/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "SkTextBlob.h"

// https://bugs.skia.org/5321
// two strings should draw the same.  PDF did not.
DEF_SIMPLE_GM(skbug_5321, canvas, 128, 128) {
    SkFont font;
    font.setEdging(SkFont::Edging::kAlias);
    font.setSize(30);

    const char text[] = "x\314\200y";  // utf8(u"x\u0300y")
    SkScalar x = 20, y = 45;

    size_t byteLength = strlen(text);
    canvas->drawSimpleText(text, byteLength, kUTF8_SkTextEncoding, x, y, font, SkPaint());

    y += font.getMetrics(nullptr);
    int glyph_count = font.countText(text, byteLength, kUTF8_SkTextEncoding);
    SkTextBlobBuilder builder;

    auto rec = builder.allocRunPosH(font, glyph_count, y);
    font.textToGlyphs(text, byteLength, kUTF8_SkTextEncoding, rec.glyphs, glyph_count);

    font.getWidths(rec.glyphs, glyph_count, rec.pos);
    for (int i = 0; i < glyph_count; ++i) {
        SkScalar w = rec.pos[i];
        rec.pos[i] = x;
        x += w;
    }

    canvas->drawTextBlob(builder.make(), 0, 0, SkPaint());
}
