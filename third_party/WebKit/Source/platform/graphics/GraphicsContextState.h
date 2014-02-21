// Copyright (C) 2013 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GraphicsContextState_h
#define GraphicsContextState_h

#include "platform/graphics/Gradient.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/Path.h"
#include "platform/graphics/Pattern.h"
#include "platform/graphics/StrokeData.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

// Encapsulates the state information we store for each pushed graphics state.
// Only GraphicsContext can use this class.
class GraphicsContextState {
    WTF_MAKE_NONCOPYABLE(GraphicsContextState);
private:
    friend class GraphicsContext;

    static PassOwnPtr<GraphicsContextState> create() { return adoptPtr(new GraphicsContextState()); }

    GraphicsContextState()
        : m_fillColor(Color::black)
        , m_fillRule(RULE_NONZERO)
        , m_textDrawingMode(TextModeFill)
        , m_alpha(1)
        , m_xferMode(nullptr)
        , m_compositeOperator(CompositeSourceOver)
        , m_blendMode(blink::WebBlendModeNormal)
#if USE(LOW_QUALITY_IMAGE_INTERPOLATION)
        , m_interpolationQuality(InterpolationLow)
#else
        , m_interpolationQuality(InterpolationHigh)
#endif
        , m_saveCount(0)
        , m_shouldAntialias(true)
        , m_shouldSmoothFonts(true)
        , m_shouldClampToSourceRect(true)
    {
    }

    void copy(GraphicsContextState* source)
    {
        m_strokeData = source->m_strokeData;
        m_fillColor = source->m_fillColor;
        m_fillRule = source->m_fillRule;
        m_fillGradient = source->m_fillGradient;
        m_fillPattern = source->m_fillPattern;
        m_looper = source->m_looper;
        m_textDrawingMode = source->m_textDrawingMode;
        m_alpha = source->m_alpha;
        m_xferMode = source->m_xferMode;
        m_colorFilter = source->m_colorFilter;
        m_compositeOperator = source->m_compositeOperator;
        m_blendMode = source->m_blendMode;
        m_interpolationQuality = source->m_interpolationQuality;
        m_saveCount = 0;
        m_shouldAntialias = source->m_shouldAntialias;
        m_shouldSmoothFonts = source->m_shouldSmoothFonts;
        m_shouldClampToSourceRect = source->m_shouldClampToSourceRect;
    }

    // Helper function for applying the state's alpha value to the given input
    // color to produce a new output color.
    SkColor applyAlpha(SkColor c) const
    {
        int s = roundf(m_alpha * 256);
        if (s >= 256)
            return c;
        if (s < 0)
            return 0;

        int a = SkAlphaMul(SkColorGetA(c), s);
        return (c & 0x00FFFFFF) | (a << 24);
    }

    // Stroke.
    StrokeData m_strokeData;

    // Fill.
    Color m_fillColor;
    WindRule m_fillRule;
    RefPtr<Gradient> m_fillGradient;
    RefPtr<Pattern> m_fillPattern;

    // Shadow. (This will need tweaking if we use draw loopers for other things.)
    RefPtr<SkDrawLooper> m_looper;

    // Text. (See TextModeFill & friends.)
    TextDrawingModeFlags m_textDrawingMode;

    // Common shader state.
    float m_alpha;
    RefPtr<SkXfermode> m_xferMode;
    RefPtr<SkColorFilter> m_colorFilter;

    // Compositing control, for the CSS and Canvas compositing spec.
    CompositeOperator m_compositeOperator;
    blink::WebBlendMode m_blendMode;

    // Image interpolation control.
    InterpolationQuality m_interpolationQuality;

    // The number of GraphicsContext::save calls since this state was pushed.
    uint16_t m_saveCount;

    bool m_shouldAntialias : 1;
    bool m_shouldSmoothFonts : 1;
    bool m_shouldClampToSourceRect : 1;
};

} // namespace WebCore

#endif // GraphicsContextState_h

