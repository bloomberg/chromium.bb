// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SkiaTextMetrics.h"

#include "wtf/MathExtras.h"

#include <SkPath.h>

namespace blink {

static hb_position_t SkiaScalarToHarfBuzzPosition(SkScalar value)
{
    // We treat HarfBuzz hb_position_t as 16.16 fixed-point.
    static const int kHbPosition1 = 1 << 16;
    return clampTo<int>(value * kHbPosition1);
}

SkiaTextMetrics::SkiaTextMetrics(const SkPaint* paint)
    : m_paint(paint)
{
    CHECK(m_paint->getTextEncoding() == SkPaint::kGlyphID_TextEncoding);
}

void SkiaTextMetrics::getGlyphWidthAndExtentsForHarfBuzz(hb_codepoint_t codepoint, hb_position_t* width, hb_glyph_extents_t* extents)
{
    DCHECK_LE(codepoint, 0xFFFFu);

    SkScalar skWidth;
    SkRect skBounds;
    uint16_t glyph = codepoint;

    m_paint->getTextWidths(&glyph, sizeof(glyph), &skWidth, &skBounds);
    if (width) {
        if (!m_paint->isSubpixelText())
            skWidth = SkScalarRoundToInt(skWidth);
        *width = SkiaScalarToHarfBuzzPosition(skWidth);
    }
    if (extents) {
        if (!m_paint->isSubpixelText()) {
            // Use roundOut() rather than round() to avoid rendering glyphs
            // outside the visual overflow rect. crbug.com/452914.
            SkIRect ir;
            skBounds.roundOut(&ir);
            skBounds.set(ir);
        }
        // Invert y-axis because Skia is y-grows-down but we set up HarfBuzz to be y-grows-up.
        extents->x_bearing = SkiaScalarToHarfBuzzPosition(skBounds.fLeft);
        extents->y_bearing = SkiaScalarToHarfBuzzPosition(-skBounds.fTop);
        extents->width = SkiaScalarToHarfBuzzPosition(skBounds.width());
        extents->height = SkiaScalarToHarfBuzzPosition(-skBounds.height());
    }
}

void SkiaTextMetrics::getSkiaBoundsForGlyph(Glyph glyph, SkRect *bounds)
{
#if OS(MACOSX)
    // TODO(drott): Remove this once we have better metrics bounds
    // on Mac, https://bugs.chromium.org/p/skia/issues/detail?id=5328
    SkPath path;
    m_paint->getTextPath(&glyph, sizeof(glyph), 0, 0, &path);
    *bounds = path.getBounds();
#else
    m_paint->getTextWidths(&glyph, sizeof(glyph), nullptr, bounds);
#endif

    if (!m_paint->isSubpixelText()) {
        SkIRect ir;
        bounds->roundOut(&ir);
        bounds->set(ir);
    }

}

float SkiaTextMetrics::getSkiaWidthForGlyph(Glyph glyph)
{
    SkScalar skWidth;
    m_paint->getTextWidths(&glyph, sizeof(glyph), &skWidth, nullptr);

    if (!m_paint->isSubpixelText())
        skWidth = SkScalarRoundToInt(skWidth);

    return SkScalarToFloat(skWidth);
}


} // namespace blink
