/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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

#include "core/platform/graphics/skia/PlatformContextSkia.h"
#include "core/platform/graphics/skia/PlatformContextSkiaState.h"

#include "core/platform/graphics/Extensions3D.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/GraphicsContext3D.h"
#include "core/platform/graphics/ImageBuffer.h"
#include "core/platform/graphics/skia/NativeImageSkia.h"
#include "core/platform/graphics/skia/SkiaUtils.h"

#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"

#include "SkBitmap.h"
#include "SkDashPathEffect.h"
#include "SkShader.h"

#include <wtf/MathExtras.h>
#include <wtf/Vector.h>

#include "core/platform/chromium/TraceEvent.h"

namespace WebCore {

struct PlatformContextSkia::DeferredSaveState {
    DeferredSaveState(unsigned mask, int count) : m_flags(mask), m_restoreCount(count) { }

    unsigned m_flags;
    int m_restoreCount;
};

// PlatformContextSkia ---------------------------------------------------------

// Danger: canvas can be NULL.
PlatformContextSkia::PlatformContextSkia(SkCanvas* canvas)
    : m_canvas(canvas)
    , m_deferredSaveFlags(0)
{
    m_stateStack.append(PlatformContextSkiaState());
    m_state = &m_stateStack.last();

    // will be assigned in setGraphicsContext()
    m_gc = 0;
}

PlatformContextSkia::~PlatformContextSkia()
{
}

void PlatformContextSkia::setCanvas(SkCanvas* canvas)
{
    m_canvas = canvas;
}

SkDevice* PlatformContextSkia::createCompatibleDevice(const IntSize& size, bool hasAlpha)
{
    return m_canvas->createCompatibleDevice(SkBitmap::kARGB_8888_Config, size.width(), size.height(), !hasAlpha);
}

void PlatformContextSkia::save()
{
    m_stateStack.append(m_state->cloneInheritedProperties());
    m_state = &m_stateStack.last();

    // The clip image only needs to be applied once. Reset the image so that we
    // don't attempt to clip multiple times.
    m_state->m_imageBufferClip.reset();

    m_saveStateStack.append(DeferredSaveState(m_deferredSaveFlags, m_canvas->getSaveCount()));
    m_deferredSaveFlags |= SkCanvas::kMatrixClip_SaveFlag;
}

void PlatformContextSkia::restore()
{
    if (!m_state->m_imageBufferClip.empty()) {
        applyClipFromImage(m_state->m_clip, m_state->m_imageBufferClip);
        m_canvas->restore();
    }

    m_stateStack.removeLast();
    m_state = &m_stateStack.last();

    DeferredSaveState savedState = m_saveStateStack.last();
    m_saveStateStack.removeLast();
    m_deferredSaveFlags = savedState.m_flags;
    m_canvas->restoreToCount(savedState.m_restoreCount);
}

void PlatformContextSkia::setStateClip(SkRect& bounds) { m_state->m_clip = bounds; }
SkRect& PlatformContextSkia::getStateClip() { return m_state->m_clip; }
void PlatformContextSkia::setStateImageBufferClip(const SkBitmap* bitmap) { m_state->m_imageBufferClip = *bitmap; }
SkBitmap* PlatformContextSkia::getStateImageBufferClip() { return &m_state->m_imageBufferClip; }

void PlatformContextSkia::drawRect(SkRect rect)
{
    SkPaint paint;
    int fillcolorNotTransparent = m_state->m_fillColor & 0xFF000000;
    if (fillcolorNotTransparent) {
        setupPaintForFilling(&paint);
        m_gc->drawRect(rect, paint);
    }

    if (m_state->m_strokeStyle != NoStroke
        && (m_state->m_strokeColor & 0xFF000000)) {
        // We do a fill of four rects to simulate the stroke of a border.
        paint.reset();
        setupPaintForFilling(&paint);
        // need to jam in the strokeColor
        paint.setColor(this->effectiveStrokeColor());

        SkRect topBorder = { rect.fLeft, rect.fTop, rect.fRight, rect.fTop + 1 };
        m_gc->drawRect(topBorder, paint);
        SkRect bottomBorder = { rect.fLeft, rect.fBottom - 1, rect.fRight, rect.fBottom };
        m_gc->drawRect(bottomBorder, paint);
        SkRect leftBorder = { rect.fLeft, rect.fTop + 1, rect.fLeft + 1, rect.fBottom - 1 };
        m_gc->drawRect(leftBorder, paint);
        SkRect rightBorder = { rect.fRight - 1, rect.fTop + 1, rect.fRight, rect.fBottom - 1 };
        m_gc->drawRect(rightBorder, paint);
    }
}

void PlatformContextSkia::setupPaintCommon(SkPaint* paint) const
{
#if defined(SK_DEBUG)
    {
        SkPaint defaultPaint;
        SkASSERT(*paint == defaultPaint);
    }
#endif

    paint->setAntiAlias(m_state->m_useAntialiasing);
    paint->setXfermodeMode(m_state->m_xferMode);
    paint->setLooper(m_state->m_looper);
}

void PlatformContextSkia::setupShader(SkPaint* paint, Gradient* grad, Pattern* pat, SkColor color) const
{
    SkShader* shader = 0;

    if (grad) {
        shader = grad->shader();
        color = SK_ColorBLACK;
    } else if (pat) {
        shader = pat->shader();
        color = SK_ColorBLACK;
        paint->setFilterBitmap(interpolationQuality() != InterpolationNone);
    }

    paint->setColor(m_state->applyAlpha(color));
    paint->setShader(shader);
}

void PlatformContextSkia::setupPaintForFilling(SkPaint* paint) const
{
    setupPaintCommon(paint);

    const GraphicsContextState& state = m_gc->state();
    setupShader(paint, state.fillGradient.get(), state.fillPattern.get(), m_state->m_fillColor);
}

float PlatformContextSkia::setupPaintForStroking(SkPaint* paint, SkRect* rect, int length) const
{
    setupPaintCommon(paint);

    const GraphicsContextState& state = m_gc->state();
    setupShader(paint, state.strokeGradient.get(), state.strokePattern.get(), m_state->m_strokeColor);

    float width = m_state->m_strokeThickness;

    paint->setStyle(SkPaint::kStroke_Style);
    paint->setStrokeWidth(SkFloatToScalar(width));
    paint->setStrokeCap(m_state->m_lineCap);
    paint->setStrokeJoin(m_state->m_lineJoin);
    paint->setStrokeMiter(SkFloatToScalar(m_state->m_miterLimit));

    if (m_state->m_dash)
        paint->setPathEffect(m_state->m_dash);
    else {
        switch (m_state->m_strokeStyle) {
        case NoStroke:
        case SolidStroke:
#if ENABLE(CSS3_TEXT)
        case DoubleStroke:
        case WavyStroke: // FIXME: https://bugs.webkit.org/show_bug.cgi?id=93509 - Needs platform support.
#endif // CSS3_TEXT
            break;
        case DashedStroke:
            width = m_state->m_dashRatio * width;
            // Fall through.
        case DottedStroke:
            // Truncate the width, since we don't want fuzzy dots or dashes.
            int dashLength = static_cast<int>(width);
            // Subtract off the endcaps, since they're rendered separately.
            int distance = length - 2 * static_cast<int>(m_state->m_strokeThickness);
            int phase = 1;
            if (dashLength > 1) {
                // Determine how many dashes or dots we should have.
                int numDashes = distance / dashLength;
                int remainder = distance % dashLength;
                // Adjust the phase to center the dashes within the line.
                if (numDashes % 2 == 0) {
                    // Even:  shift right half a dash, minus half the remainder
                    phase = (dashLength - remainder) / 2;
                } else {
                    // Odd:  shift right a full dash, minus half the remainder
                    phase = dashLength - remainder / 2;
                }
            }
            SkScalar dashLengthSk = SkIntToScalar(dashLength);
            SkScalar intervals[2] = { dashLengthSk, dashLengthSk };
            paint->setPathEffect(new SkDashPathEffect(intervals, 2, SkIntToScalar(phase)))->unref();
        }
    }

    return width;
}

void PlatformContextSkia::setDrawLooper(SkDrawLooper* dl)
{
    SkRefCnt_SafeAssign(m_state->m_looper, dl);
}

void PlatformContextSkia::setMiterLimit(float ml)
{
    m_state->m_miterLimit = ml;
}

void PlatformContextSkia::setAlpha(float alpha)
{
    m_state->m_alpha = alpha;
}

void PlatformContextSkia::setLineCap(SkPaint::Cap lc)
{
    m_state->m_lineCap = lc;
}

void PlatformContextSkia::setLineJoin(SkPaint::Join lj)
{
    m_state->m_lineJoin = lj;
}

void PlatformContextSkia::setXfermodeMode(SkXfermode::Mode pdm)
{
    m_state->m_xferMode = pdm;
}

void PlatformContextSkia::setFillColor(SkColor color)
{
    m_state->m_fillColor = color;
}

SkDrawLooper* PlatformContextSkia::getDrawLooper() const
{
    return m_state->m_looper;
}

StrokeStyle PlatformContextSkia::getStrokeStyle() const
{
    return m_state->m_strokeStyle;
}

void PlatformContextSkia::setStrokeStyle(StrokeStyle strokeStyle)
{
    m_state->m_strokeStyle = strokeStyle;
}

void PlatformContextSkia::setStrokeColor(SkColor strokeColor)
{
    m_state->m_strokeColor = strokeColor;
}

float PlatformContextSkia::getStrokeThickness() const
{
    return m_state->m_strokeThickness;
}

void PlatformContextSkia::setStrokeThickness(float thickness)
{
    m_state->m_strokeThickness = thickness;
}

TextDrawingModeFlags PlatformContextSkia::getTextDrawingMode() const
{
    return m_state->m_textDrawingMode;
}

float PlatformContextSkia::getAlpha() const
{
    return m_state->m_alpha;
}

int PlatformContextSkia::getNormalizedAlpha() const
{
    int alpha = roundf(m_state->m_alpha * 256);
    if (alpha > 255)
        alpha = 255;
    else if (alpha < 0)
        alpha = 0;
    return alpha;
}

SkXfermode::Mode PlatformContextSkia::getXfermodeMode() const
{
    return m_state->m_xferMode;
}

void PlatformContextSkia::setTextDrawingMode(TextDrawingModeFlags mode)
{
    m_state->m_textDrawingMode = mode;
}

void PlatformContextSkia::setUseAntialiasing(bool enable)
{
    m_state->m_useAntialiasing = enable;
}

SkColor PlatformContextSkia::effectiveFillColor() const
{
    return m_state->applyAlpha(m_state->m_fillColor);
}

SkColor PlatformContextSkia::effectiveStrokeColor() const
{
    return m_state->applyAlpha(m_state->m_strokeColor);
}

InterpolationQuality PlatformContextSkia::interpolationQuality() const
{
    return m_state->m_interpolationQuality;
}

void PlatformContextSkia::setInterpolationQuality(InterpolationQuality interpolationQuality)
{
    m_state->m_interpolationQuality = interpolationQuality;
}

void PlatformContextSkia::setDashPathEffect(SkDashPathEffect* dash)
{
    if (dash != m_state->m_dash) {
        SkSafeUnref(m_state->m_dash);
        m_state->m_dash = dash;
    }
}

const SkBitmap* PlatformContextSkia::bitmap() const
{
    TRACE_EVENT0("skia", "PlatformContextSkia::bitmap");
    return &m_canvas->getDevice()->accessBitmap(false);
}

bool PlatformContextSkia::isNativeFontRenderingAllowed()
{
    return false;
}

void PlatformContextSkia::applyClipFromImage(const SkRect& rect, const SkBitmap& imageBuffer)
{
    // NOTE: this assumes the image mask contains opaque black for the portions that are to be shown, as such we
    // only look at the alpha when compositing. I'm not 100% sure this is what WebKit expects for image clipping.
    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
    realizeSave(SkCanvas::kMatrixClip_SaveFlag);
    m_canvas->save(SkCanvas::kMatrix_SaveFlag);
    m_canvas->resetMatrix();
    m_canvas->drawBitmapRect(imageBuffer, 0, rect, &paint);
    m_canvas->restore();
}

void PlatformContextSkia::adjustTextRenderMode(SkPaint* paint)
{
    if (!paint->isLCDRenderText())
        return;

    paint->setLCDRenderText(couldUseLCDRenderedText());
}

bool PlatformContextSkia::couldUseLCDRenderedText()
{
    // Our layers only have a single alpha channel. This means that subpixel
    // rendered text cannot be composited correctly when the layer is
    // collapsed. Therefore, subpixel text is disabled when we are drawing
    // onto a layer.
    if (isDrawingToLayer())
        return false;

    return m_gc->shouldSmoothFonts();
}

} // namespace WebCore
