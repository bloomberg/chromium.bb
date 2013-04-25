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

#ifndef PlatformContextSkiaState_h
#define PlatformContextSkiaState_h

#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"

namespace WebCore {

// Encapsulates the additional painting state information we store for each
// pushed graphics state.
class PlatformContextSkiaState {
public:
    PlatformContextSkiaState()
        : m_alpha(1)
        , m_xferMode(SkXfermode::kSrcOver_Mode)
        , m_useAntialiasing(true)
        , m_looper(0)
        , m_fillColor(0xFF000000)
        , m_strokeStyle(SolidStroke)
        , m_strokeColor(Color::black)
        , m_strokeThickness(0)
        , m_dashRatio(3)
        , m_miterLimit(4)
        , m_lineCap(SkPaint::kDefault_Cap)
        , m_lineJoin(SkPaint::kDefault_Join)
        , m_dash(0)
        , m_textDrawingMode(TextModeFill)
        , m_clip(SkRect::MakeEmpty())
#if USE(LOW_QUALITY_IMAGE_INTERPOLATION)
        , m_interpolationQuality(InterpolationLow)
#else
        , m_interpolationQuality(InterpolationHigh)
#endif
    {
    }

    PlatformContextSkiaState(const PlatformContextSkiaState& other)
        : m_alpha(other.m_alpha)
        , m_xferMode(other.m_xferMode)
        , m_useAntialiasing(other.m_useAntialiasing)
        , m_looper(other.m_looper)
        , m_fillColor(other.m_fillColor)
        , m_strokeStyle(other.m_strokeStyle)
        , m_strokeColor(other.m_strokeColor)
        , m_strokeThickness(other.m_strokeThickness)
        , m_dashRatio(other.m_dashRatio)
        , m_miterLimit(other.m_miterLimit)
        , m_lineCap(other.m_lineCap)
        , m_lineJoin(other.m_lineJoin)
        , m_dash(other.m_dash)
        , m_textDrawingMode(other.m_textDrawingMode)
        , m_imageBufferClip(other.m_imageBufferClip)
        , m_clip(other.m_clip)
        , m_interpolationQuality(other.m_interpolationQuality)
    {
        // Up the ref count of these. SkSafeRef does nothing if its argument is 0.
        SkSafeRef(m_looper);
        SkSafeRef(m_dash);
    }

    ~PlatformContextSkiaState()
    {
        SkSafeUnref(m_looper);
        SkSafeUnref(m_dash);
    }

    // Common shader state.
    float m_alpha;
    SkXfermode::Mode m_xferMode;
    bool m_useAntialiasing;
    SkDrawLooper* m_looper;

    // Fill.
    SkColor m_fillColor;

    // Stroke.
    StrokeStyle m_strokeStyle;
    SkColor m_strokeColor;
    float m_strokeThickness;
    int m_dashRatio;  // Ratio of the length of a dash to its width.
    float m_miterLimit;
    SkPaint::Cap m_lineCap;
    SkPaint::Join m_lineJoin;
    SkDashPathEffect* m_dash;

    // Text. (See TextModeFill & friends in GraphicsContext.h.)
    TextDrawingModeFlags m_textDrawingMode;

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

    // If non-empty, the current State is clipped to this image.
    SkBitmap m_imageBufferClip;
    // If m_imageBufferClip is non-empty, this is the region the image is clipped to.
    SkRect m_clip;

    InterpolationQuality m_interpolationQuality;

    // Returns a new State with all of this object's inherited properties copied.
    PlatformContextSkiaState cloneInheritedProperties() { return PlatformContextSkiaState(*this); }

private:
    // Not supported.
    void operator=(const PlatformContextSkiaState&);
};

} // namespace WebCore

#endif // PlatformContextSkiaState_h

