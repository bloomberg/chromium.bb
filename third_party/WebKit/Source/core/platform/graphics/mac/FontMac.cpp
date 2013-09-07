/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/platform/graphics/Font.h"

#include "core/platform/LayoutTestSupport.h"
#include "core/platform/graphics/FontSmoothingMode.h"
#include "core/platform/graphics/GlyphBuffer.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/SimpleFontData.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkTypeface_mac.h"

namespace WebCore {

bool Font::canReturnFallbackFontsForComplexText()
{
    return true;
}

bool Font::canExpandAroundIdeographsInComplexText()
{
    return true;
}

static void setupPaint(SkPaint* paint, const SimpleFontData* fontData, const Font* font, bool shouldAntialias, bool shouldSmoothFonts)
{
    const FontPlatformData& platformData = fontData->platformData();
    const float textSize = platformData.m_size >= 0 ? platformData.m_size : 12;

    paint->setAntiAlias(shouldAntialias);
    paint->setEmbeddedBitmapText(false);
    paint->setTextSize(SkFloatToScalar(textSize));
    paint->setVerticalText(platformData.orientation() == Vertical);
    SkTypeface* typeface = SkCreateTypefaceFromCTFont(platformData.ctFont());
    SkAutoUnref autoUnref(typeface);
    paint->setTypeface(typeface);
    paint->setFakeBoldText(platformData.m_syntheticBold);
    paint->setTextSkewX(platformData.m_syntheticOblique ? -SK_Scalar1 / 4 : 0);
    paint->setAutohinted(false); // freetype specific
    paint->setLCDRenderText(shouldSmoothFonts);
    paint->setSubpixelText(true);

#if OS(MACOSX)
    // When using CoreGraphics, disable hinting when webkit-font-smoothing:antialiased is used.
    // See crbug.com/152304
    if (font->fontDescription().fontSmoothing() == Antialiased)
        paint->setHinting(SkPaint::kNo_Hinting);
#endif

    if (font->fontDescription().textRenderingMode() == GeometricPrecision)
        paint->setHinting(SkPaint::kNo_Hinting);
}

// TODO: This needs to be split into helper functions to better scope the
// inputs/outputs, and reduce duplicate code.
// This issue is tracked in https://bugs.webkit.org/show_bug.cgi?id=62989
void Font::drawGlyphs(GraphicsContext* gc, const SimpleFontData* font,
                      const GlyphBuffer& glyphBuffer,  int from, int numGlyphs,
                      const FloatPoint& point, const FloatRect& textRect) const {
    COMPILE_ASSERT(sizeof(GlyphBufferGlyph) == sizeof(uint16_t), GlyphBufferGlyphSize_equals_uint16_t);

    bool shouldSmoothFonts = true;
    bool shouldAntialias = true;

    switch (fontDescription().fontSmoothing()) {
    case Antialiased:
        shouldSmoothFonts = false;
        break;
    case SubpixelAntialiased:
        break;
    case NoSmoothing:
        shouldAntialias = false;
        shouldSmoothFonts = false;
        break;
    case AutoSmoothing:
        // For the AutoSmooth case, don't do anything! Keep the default settings.
        break;
    }

    if (!shouldUseSmoothing() || isRunningLayoutTest()) {
        shouldSmoothFonts = false;
        shouldAntialias = false;
    }

    const GlyphBufferGlyph* glyphs = glyphBuffer.glyphs(from);
    SkScalar x = SkFloatToScalar(point.x());
    SkScalar y = SkFloatToScalar(point.y());

    if (font->platformData().orientation() == Vertical)
        y += SkFloatToScalar(font->fontMetrics().floatAscent(IdeographicBaseline) - font->fontMetrics().floatAscent());
    // FIXME: text rendering speed:
    // Android has code in their WebCore fork to special case when the
    // GlyphBuffer has no advances other than the defaults. In that case the
    // text drawing can proceed faster. However, it's unclear when those
    // patches may be upstreamed to WebKit so we always use the slower path
    // here.
    const GlyphBufferAdvance* adv = glyphBuffer.advances(from);
    SkAutoSTMalloc<32, SkPoint> storage(numGlyphs);
    SkPoint* pos = storage.get();

    for (int i = 0; i < numGlyphs; i++) {
        pos[i].set(x, y);
        x += SkFloatToScalar(adv[i].width());
        y += SkFloatToScalar(adv[i].height());
    }

    if (font->platformData().orientation() == Vertical) {
        gc->save();
        gc->rotate(-0.5 * SK_ScalarPI);
        SkMatrix rotator;
        rotator.reset();
        rotator.setRotate(90);
        rotator.mapPoints(pos, numGlyphs);
    }
    TextDrawingModeFlags textMode = gc->textDrawingMode();

    // We draw text up to two times (once for fill, once for stroke).
    if (textMode & TextModeFill) {
        SkPaint paint;
        gc->setupPaintForFilling(&paint);
        setupPaint(&paint, font, this, shouldAntialias, shouldSmoothFonts);
        gc->adjustTextRenderMode(&paint);
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

        gc->drawPosText(glyphs, numGlyphs * sizeof(uint16_t), pos, textRect, paint);
    }

    if ((textMode & TextModeStroke)
        && gc->strokeStyle() != NoStroke
        && gc->strokeThickness() > 0) {

        SkPaint paint;
        gc->setupPaintForStroking(&paint);
        setupPaint(&paint, font, this, shouldAntialias, shouldSmoothFonts);
        gc->adjustTextRenderMode(&paint);
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

        if (textMode & TextModeFill) {
            // If we also filled, we don't want to draw shadows twice.
            // See comment in FontChromiumWin.cpp::paintSkiaText() for more details.
            paint.setLooper(0);
        }

        gc->drawPosText(glyphs, numGlyphs * sizeof(uint16_t), pos, textRect, paint);
    }
    if (font->platformData().orientation() == Vertical)
        gc->restore();
}

} // namespace WebCore
