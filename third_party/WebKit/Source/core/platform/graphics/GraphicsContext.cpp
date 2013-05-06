/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/platform/graphics/GraphicsContext.h"

#include "core/platform/KURL.h"
#include "core/platform/graphics/BitmapImage.h"
#include "core/platform/graphics/Generator.h"
#include "core/platform/graphics/ImageBuffer.h"
#include "core/platform/graphics/IntRect.h"
#include "core/platform/graphics/RoundedRect.h"
#include "core/platform/graphics/TextRunIterator.h"
#include "core/platform/graphics/skia/SkiaUtils.h"
#include "core/platform/graphics/skia/PlatformContextSkia.h"
#include "core/platform/text/BidiResolver.h"

#include "third_party/skia/include/core/SkAnnotation.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "third_party/skia/include/effects/SkLayerDrawLooper.h"

#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>

#if OS(DARWIN)
#include <ApplicationServices/ApplicationServices.h>
#endif

using namespace std;

namespace WebCore {

GraphicsContext::GraphicsContext(SkCanvas* canvas)
    : m_transparencyCount(0)
    , m_trackOpaqueRegion(false)
    , m_useHighResMarker(false)
    , m_updatingControlTints(false)
    , m_accelerated(false)
    , m_isCertainlyOpaque(true)
    , m_printing(false)
{
    if (canvas) {
        m_data = adoptPtr(new PlatformContextSkia(canvas));
        m_data->setGraphicsContext(this);
    } else {
        // the caller owns the gc
        setPaintingDisabled(true);
    }
}

GraphicsContext::~GraphicsContext()
{
    ASSERT(m_stack.isEmpty());
    ASSERT(!m_transparencyCount);
}

PlatformGraphicsContext* GraphicsContext::platformContext() const
{
    ASSERT(!paintingDisabled());
    return m_data.get();
}

void GraphicsContext::save()
{
    if (paintingDisabled())
        return;

    m_stack.append(m_state);

    // Save our private State.
    platformContext()->save();
}

void GraphicsContext::restore()
{
    if (paintingDisabled())
        return;

    if (m_stack.isEmpty()) {
        LOG_ERROR("ERROR void GraphicsContext::restore() stack is empty");
        return;
    }
    m_state = m_stack.last();
    m_stack.removeLast();

    // Restore our private State.
    platformContext()->restore();
}

void GraphicsContext::saveLayer(const SkRect* bounds, const SkPaint* paint, SkCanvas::SaveFlags saveFlags)
{
    platformContext()->saveLayer();

    platformContext()->canvas()->saveLayer(bounds, paint, saveFlags);
    if (bounds)
        platformContext()->canvas()->clipRect(*bounds);
    if (m_trackOpaqueRegion)
        m_opaqueRegion.pushCanvasLayer(paint);
}

void GraphicsContext::restoreLayer()
{
    platformContext()->canvas()->restore();
    if (m_trackOpaqueRegion)
        m_opaqueRegion.popCanvasLayer(this);
}

void GraphicsContext::setStrokeThickness(float thickness)
{
    m_state.strokeThickness = thickness;

    if (paintingDisabled())
        return;
    platformContext()->setStrokeThickness(thickness);
}

void GraphicsContext::setStrokeStyle(StrokeStyle style)
{
    m_state.strokeStyle = style;

    if (paintingDisabled())
        return;

    platformContext()->setStrokeStyle(style);
}

void GraphicsContext::setStrokeColor(const Color& color, ColorSpace colorSpace)
{
    m_state.strokeColor = color;
    m_state.strokeColorSpace = colorSpace;
    m_state.strokeGradient.clear();
    m_state.strokePattern.clear();

    if (paintingDisabled())
        return;

    platformContext()->setStrokeColor(color.rgb());
}

void GraphicsContext::setShadow(const FloatSize& size, float blur, const Color& color, ColorSpace colorSpace)
{
    m_state.shadowOffset = size;
    m_state.shadowBlur = blur;
    m_state.shadowColor = color;
    m_state.shadowColorSpace = colorSpace;

    if (paintingDisabled())
        return;

    // Detect when there's no effective shadow and clear the looper.
    if (!size.width() && !size.height() && !blur) {
        platformContext()->setDrawLooper(0);
        return;
    }

    double width = size.width();
    double height = size.height();

    uint32_t mfFlags = SkBlurMaskFilter::kHighQuality_BlurFlag;

    if (m_state.shadowsIgnoreTransforms)  {
        // Currently only the GraphicsContext associated with the
        // CanvasRenderingContext for HTMLCanvasElement have shadows ignore
        // Transforms. So with this flag set, we know this state is associated
        // with a CanvasRenderingContext.
        mfFlags |= SkBlurMaskFilter::kIgnoreTransform_BlurFlag;

        // CG uses natural orientation for Y axis, but the HTML5 canvas spec
        // does not.
        // So we now flip the height since it was flipped in
        // CanvasRenderingContext in order to work with CG.
        height = -height;
    }

    SkColor c;
    if (color.isValid())
        c = color.rgb();
    else
        c = SkColorSetARGB(0xFF/3, 0, 0, 0);    // "std" apple shadow color.

    // TODO(tc): Should we have a max value for the blur?  CG clamps at 1000.0
    // for perf reasons.

    SkLayerDrawLooper* dl = new SkLayerDrawLooper;
    SkAutoUnref aur(dl);

    // top layer, we just draw unchanged
    dl->addLayer();

    // lower layer contains our offset, blur, and colorfilter
    SkLayerDrawLooper::LayerInfo info;

    // Since CSS box-shadow ignores the original alpha, we used to default to kSrc_Mode here and
    // only switch to kDst_Mode for Canvas. But that precaution is not really necessary because
    // RenderBoxModelObject performs a dedicated shadow fill with opaque black to ensure
    // cross-platform consistent results.
    info.fColorMode = SkXfermode::kDst_Mode;
    info.fPaintBits |= SkLayerDrawLooper::kMaskFilter_Bit; // our blur
    info.fPaintBits |= SkLayerDrawLooper::kColorFilter_Bit;
    info.fOffset.set(width, height);
    info.fPostTranslate = m_state.shadowsIgnoreTransforms;

    SkMaskFilter* mf = SkBlurMaskFilter::Create((double)blur / 2.0, SkBlurMaskFilter::kNormal_BlurStyle, mfFlags);

    SkColorFilter* cf = SkColorFilter::CreateModeFilter(c, SkXfermode::kSrcIn_Mode);

    SkPaint* paint = dl->addLayer(info);
    SkSafeUnref(paint->setMaskFilter(mf));
    SkSafeUnref(paint->setColorFilter(cf));

    // dl is now built, just install it
    platformContext()->setDrawLooper(dl);
}

void GraphicsContext::setLegacyShadow(const FloatSize& offset, float blur, const Color& color, ColorSpace colorSpace)
{
    setShadow(offset, blur, color, colorSpace);
}

void GraphicsContext::clearShadow()
{
    m_state.shadowOffset = FloatSize();
    m_state.shadowBlur = 0;
    m_state.shadowColor = Color();
    m_state.shadowColorSpace = ColorSpaceDeviceRGB;

    if (paintingDisabled())
        return;
    platformContext()->setDrawLooper(0);
}

bool GraphicsContext::hasShadow() const
{
    return m_state.shadowColor.isValid() && m_state.shadowColor.alpha()
           && (m_state.shadowBlur || m_state.shadowOffset.width() || m_state.shadowOffset.height());
}

bool GraphicsContext::getShadow(FloatSize& offset, float& blur, Color& color, ColorSpace& colorSpace) const
{
    offset = m_state.shadowOffset;
    blur = m_state.shadowBlur;
    color = m_state.shadowColor;
    colorSpace = m_state.shadowColorSpace;

    return hasShadow();
}

float GraphicsContext::strokeThickness() const
{
    return m_state.strokeThickness;
}

StrokeStyle GraphicsContext::strokeStyle() const
{
    return m_state.strokeStyle;
}

Color GraphicsContext::strokeColor() const
{
    return m_state.strokeColor;
}

ColorSpace GraphicsContext::strokeColorSpace() const
{
    return m_state.strokeColorSpace;
}

WindRule GraphicsContext::fillRule() const
{
    return m_state.fillRule;
}

void GraphicsContext::setFillRule(WindRule fillRule)
{
    m_state.fillRule = fillRule;
}

void GraphicsContext::setFillColor(const Color& color, ColorSpace colorSpace)
{
    m_state.fillColor = color;
    m_state.fillColorSpace = colorSpace;
    m_state.fillGradient.clear();
    m_state.fillPattern.clear();

    if (paintingDisabled())
        return;

    platformContext()->setFillColor(color.rgb());
}

Color GraphicsContext::fillColor() const
{
    return m_state.fillColor;
}

ColorSpace GraphicsContext::fillColorSpace() const
{
    return m_state.fillColorSpace;
}

void GraphicsContext::setShouldAntialias(bool b)
{
    m_state.shouldAntialias = b;

    if (paintingDisabled())
        return;

    platformContext()->setUseAntialiasing(b);
}

bool GraphicsContext::shouldAntialias() const
{
    return m_state.shouldAntialias;
}

void GraphicsContext::setShouldSmoothFonts(bool b)
{
    m_state.shouldSmoothFonts = b;
}

bool GraphicsContext::shouldSmoothFonts() const
{
    return m_state.shouldSmoothFonts;
}

void GraphicsContext::setShouldSubpixelQuantizeFonts(bool b)
{
    m_state.shouldSubpixelQuantizeFonts = b;
}

bool GraphicsContext::shouldSubpixelQuantizeFonts() const
{
    return m_state.shouldSubpixelQuantizeFonts;
}

const GraphicsContextState& GraphicsContext::state() const
{
    return m_state;
}

void GraphicsContext::setStrokePattern(PassRefPtr<Pattern> pattern)
{
    ASSERT(pattern);
    if (!pattern) {
        setStrokeColor(Color::black, ColorSpaceDeviceRGB);
        return;
    }
    m_state.strokeGradient.clear();
    m_state.strokePattern = pattern;
}

void GraphicsContext::setFillPattern(PassRefPtr<Pattern> pattern)
{
    ASSERT(pattern);
    if (!pattern) {
        setFillColor(Color::black, ColorSpaceDeviceRGB);
        return;
    }
    m_state.fillGradient.clear();
    m_state.fillPattern = pattern;
}

void GraphicsContext::setStrokeGradient(PassRefPtr<Gradient> gradient)
{
    ASSERT(gradient);
    if (!gradient) {
        setStrokeColor(Color::black, ColorSpaceDeviceRGB);
        return;
    }
    m_state.strokeGradient = gradient;
    m_state.strokePattern.clear();
}

void GraphicsContext::setFillGradient(PassRefPtr<Gradient> gradient)
{
    ASSERT(gradient);
    if (!gradient) {
        setFillColor(Color::black, ColorSpaceDeviceRGB);
        return;
    }
    m_state.fillGradient = gradient;
    m_state.fillPattern.clear();
}

Gradient* GraphicsContext::fillGradient() const
{
    return m_state.fillGradient.get();
}

Gradient* GraphicsContext::strokeGradient() const
{
    return m_state.strokeGradient.get();
}

Pattern* GraphicsContext::fillPattern() const
{
    return m_state.fillPattern.get();
}

Pattern* GraphicsContext::strokePattern() const
{
    return m_state.strokePattern.get();
}

void GraphicsContext::setShadowsIgnoreTransforms(bool ignoreTransforms)
{
    m_state.shadowsIgnoreTransforms = ignoreTransforms;
}

bool GraphicsContext::shadowsIgnoreTransforms() const
{
    return m_state.shadowsIgnoreTransforms;
}

void GraphicsContext::setLineCap(LineCap cap)
{
    if (paintingDisabled())
        return;
    switch (cap) {
    case ButtCap:
        platformContext()->setLineCap(SkPaint::kButt_Cap);
        break;
    case RoundCap:
        platformContext()->setLineCap(SkPaint::kRound_Cap);
        break;
    case SquareCap:
        platformContext()->setLineCap(SkPaint::kSquare_Cap);
        break;
    default:
        ASSERT(0);
        break;
    }
}

void GraphicsContext::setLineDash(const DashArray& dashes, float dashOffset)
{
    if (paintingDisabled())
        return;

    // FIXME: This is lifted directly off SkiaSupport, lines 49-74
    // so it is not guaranteed to work correctly.
    size_t dashLength = dashes.size();
    if (!dashLength) {
        // If no dash is set, revert to solid stroke
        // FIXME: do we need to set NoStroke in some cases?
        platformContext()->setStrokeStyle(SolidStroke);
        platformContext()->setDashPathEffect(0);
        return;
    }

    size_t count = !(dashLength % 2) ? dashLength : dashLength * 2;
    SkScalar* intervals = new SkScalar[count];

    for (unsigned int i = 0; i < count; i++)
        intervals[i] = dashes[i % dashLength];

    platformContext()->setDashPathEffect(new SkDashPathEffect(intervals, count, dashOffset));

    delete[] intervals;
}

void GraphicsContext::setLineJoin(LineJoin join)
{
    if (paintingDisabled())
        return;
    switch (join) {
    case MiterJoin:
        platformContext()->setLineJoin(SkPaint::kMiter_Join);
        break;
    case RoundJoin:
        platformContext()->setLineJoin(SkPaint::kRound_Join);
        break;
    case BevelJoin:
        platformContext()->setLineJoin(SkPaint::kBevel_Join);
        break;
    default:
        ASSERT(0);
        break;
    }
}

void GraphicsContext::setMiterLimit(float limit)
{
    if (paintingDisabled())
        return;
    platformContext()->setMiterLimit(limit);
}

void GraphicsContext::setAlpha(float alpha)
{
    if (paintingDisabled())
        return;

    platformContext()->setAlpha(alpha);
}

bool GraphicsContext::supportsTransparencyLayers()
{
    return true;
}

bool GraphicsContext::isInTransparencyLayer() const
{
    return (m_transparencyCount > 0) && supportsTransparencyLayers();
}

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    // We need the "alpha" layer flag here because the base layer is opaque
    // (the surface of the page) but layers on top may have transparent parts.
    // Without explicitly setting the alpha flag, the layer will inherit the
    // opaque setting of the base and some things won't work properly.
    SkCanvas::SaveFlags saveFlags = static_cast<SkCanvas::SaveFlags>(SkCanvas::kHasAlphaLayer_SaveFlag | SkCanvas::kFullColorLayer_SaveFlag);

    SkPaint layerPaint;
    layerPaint.setAlpha(static_cast<unsigned char>(opacity * 255));
    layerPaint.setXfermodeMode(platformContext()->getXfermodeMode());

    saveLayer(0, &layerPaint, saveFlags);

    ++m_transparencyCount;
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;

    restoreLayer();

    ASSERT(m_transparencyCount > 0);
    --m_transparencyCount;
}

void GraphicsContext::beginLayerClippedToImage(const FloatRect& rect, const ImageBuffer* imageBuffer)
{
    SkRect bounds = WebCoreFloatRectToSKRect(rect);

    if (imageBuffer->internalSize().isEmpty()) {
        platformContext()->clipRect(bounds);
        return;
    }

    // Skia doesn't support clipping to an image, so we create a layer. The next
    // time restore is invoked the layer and |imageBuffer| are combined to
    // create the resulting image.
    platformContext()->setStateClip(bounds);

    // Get the absolute coordinates of the stored clipping rectangle to make it
    // independent of any transform changes.
    platformContext()->getTotalMatrix().mapRect(&platformContext()->getStateClip());

    SkCanvas::SaveFlags saveFlags = static_cast<SkCanvas::SaveFlags>(SkCanvas::kHasAlphaLayer_SaveFlag | SkCanvas::kFullColorLayer_SaveFlag);
    saveLayer(&bounds, 0, saveFlags);

    const SkBitmap* bitmap = imageBuffer->context()->platformContext()->bitmap();

    if (m_trackOpaqueRegion) {
        SkRect opaqueRect = bitmap->isOpaque() ? platformContext()->getStateClip() : SkRect::MakeEmpty();
        m_opaqueRegion.setImageMask(opaqueRect);
    }

    // Copy off the image as |imageBuffer| may be deleted before restore is invoked.
    if (bitmap->isImmutable())
        platformContext()->setStateImageBufferClip(bitmap);
    else {
        // We need to make a deep-copy of the pixels themselves, so they don't
        // change on us between now and when we want to apply them in restore()
        bitmap->copyTo(platformContext()->getStateImageBufferClip(), SkBitmap::kARGB_8888_Config);
    }
}

bool GraphicsContext::updatingControlTints() const
{
    return m_updatingControlTints;
}

void GraphicsContext::setUpdatingControlTints(bool b)
{
    setPaintingDisabled(b);
    m_updatingControlTints = b;
}

void GraphicsContext::setPaintingDisabled(bool f)
{
    m_state.paintingDisabled = f;
}

bool GraphicsContext::paintingDisabled() const
{
    return m_state.paintingDisabled;
}

InterpolationQuality GraphicsContext::imageInterpolationQuality() const
{
    return platformContext()->interpolationQuality();
}

void GraphicsContext::setImageInterpolationQuality(InterpolationQuality q)
{
    platformContext()->setInterpolationQuality(q);
}

void GraphicsContext::drawConvexPolygon(size_t numPoints, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;

    SkPath path;
    setPathFromConvexPoints(&path, numPoints, points);

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    paint.setAntiAlias(shouldAntialias);
    drawPath(path, paint);

    if (strokeStyle() != NoStroke) {
        paint.reset();
        platformContext()->setupPaintForStroking(&paint, 0, 0);
        drawPath(path, paint);
    }
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& elipseRect)
{
    if (paintingDisabled())
        return;

    SkRect rect = elipseRect;
    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    drawOval(rect, paint);

    if (strokeStyle() != NoStroke) {
        paint.reset();
        platformContext()->setupPaintForStroking(&paint, &rect, 0);
        drawOval(rect, paint);
    }
}

void GraphicsContext::drawFocusRing(const Path& path, int width, int offset, const Color& color)
{
    // FIXME: implement
}

void GraphicsContext::drawFocusRing(const Vector<IntRect>& rects, int width, int offset, const Color& color)
{
    if (paintingDisabled())
        return;

    unsigned rectCount = rects.size();
    if (!rectCount)
        return;

    SkRegion focusRingRegion;
    const int focusRingOutset = getFocusRingOutset(offset);
    for (unsigned i = 0; i < rectCount; i++) {
        SkIRect r = rects[i];
        r.inset(-focusRingOutset, -focusRingOutset);
        focusRingRegion.op(r, SkRegion::kUnion_Op);
    }

    SkPath path;
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);

    paint.setColor(color.rgb());
    focusRingRegion.getBoundaryPath(&path);
    drawOuterPath(path, paint, width);
    drawInnerPath(path, paint, width);
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    StrokeStyle penStyle = strokeStyle();
    if (penStyle == NoStroke)
        return;

    SkPaint paint;
    FloatPoint p1 = point1;
    FloatPoint p2 = point2;
    bool isVerticalLine = (p1.x() == p2.x());
    int width = roundf(strokeThickness());

    // We know these are vertical or horizontal lines, so the length will just
    // be the sum of the displacement component vectors give or take 1 -
    // probably worth the speed up of no square root, which also won't be exact.
    FloatSize disp = p2 - p1;
    int length = SkScalarRound(disp.width() + disp.height());
    platformContext()->setupPaintForStroking(&paint, 0, length);

    if (strokeStyle() == DottedStroke || strokeStyle() == DashedStroke) {
        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.

        SkRect r1, r2;
        r1.set(p1.x(), p1.y(), p1.x() + width, p1.y() + width);
        r2.set(p2.x(), p2.y(), p2.x() + width, p2.y() + width);

        if (isVerticalLine) {
            r1.offset(-width / 2, 0);
            r2.offset(-width / 2, -width);
        } else {
            r1.offset(0, -width / 2);
            r2.offset(-width, -width / 2);
        }
        SkPaint fillPaint;
        fillPaint.setColor(paint.getColor());
        drawRect(r1, fillPaint);
        drawRect(r2, fillPaint);
    }

    adjustLineToPixelBoundaries(p1, p2, width, penStyle);
    SkPoint pts[2] = { (SkPoint)p1, (SkPoint)p2 };

    platformContext()->canvas()->drawPoints(SkCanvas::kLines_PointMode, 2, pts, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawPoints(this, SkCanvas::kLines_PointMode, 2, pts, paint);
}

void GraphicsContext::drawLineForDocumentMarker(const FloatPoint& pt, float width, DocumentMarkerLineStyle style)
{
    if (paintingDisabled())
        return;

    int deviceScaleFactor = m_useHighResMarker ? 2 : 1;

    // Create the pattern we'll use to draw the underline.
    int index = style == DocumentMarkerGrammarLineStyle ? 1 : 0;
    static SkBitmap* misspellBitmap1x[2] = { 0, 0 };
    static SkBitmap* misspellBitmap2x[2] = { 0, 0 };
    SkBitmap** misspellBitmap = deviceScaleFactor == 2 ? misspellBitmap2x : misspellBitmap1x;
    if (!misspellBitmap[index]) {
#if OS(DARWIN)
        // Match the artwork used by the Mac.
        const int rowPixels = 4 * deviceScaleFactor;
        const int colPixels = 3 * deviceScaleFactor;
        misspellBitmap[index] = new SkBitmap;
        misspellBitmap[index]->setConfig(SkBitmap::kARGB_8888_Config,
                                         rowPixels, colPixels);
        misspellBitmap[index]->allocPixels();

        misspellBitmap[index]->eraseARGB(0, 0, 0, 0);
        const uint32_t transparentColor = 0x00000000;

        if (deviceScaleFactor == 1) {
            const uint32_t colors[2][6] = {
                { 0x2A2A0600, 0x57571000,  0xA8A81B00, 0xBFBF1F00,  0x70701200, 0xE0E02400 },
                { 0x2A001503, 0x57002A08,  0xA800540D, 0xBF005F0F,  0x70003809, 0xE0007012 }
            };

            // Pattern: a b a   a b a
            //          c d c   c d c
            //          e f e   e f e
            for (int x = 0; x < colPixels; ++x) {
                uint32_t* row = misspellBitmap[index]->getAddr32(0, x);
                row[0] = colors[index][x * 2];
                row[1] = colors[index][x * 2 + 1];
                row[2] = colors[index][x * 2];
                row[3] = transparentColor;
            }
        } else if (deviceScaleFactor == 2) {
            const uint32_t colors[2][18] = {
                { 0x0a090101, 0x33320806, 0x55540f0a,  0x37360906, 0x6e6c120c, 0x6e6c120c,  0x7674140d, 0x8d8b1810, 0x8d8b1810,
                  0x96941a11, 0xb3b01f15, 0xb3b01f15,  0x6d6b130c, 0xd9d62619, 0xd9d62619,  0x19180402, 0x7c7a150e, 0xcecb2418 },
                { 0x0a000400, 0x33031b06, 0x55062f0b,  0x37041e06, 0x6e083d0d, 0x6e083d0d,  0x7608410e, 0x8d094e11, 0x8d094e11,
                  0x960a5313, 0xb30d6417, 0xb30d6417,  0x6d073c0d, 0xd90f781c, 0xd90f781c,  0x19010d03, 0x7c094510, 0xce0f731a }
            };

            // Pattern: a b c c b a
            //          d e f f e d
            //          g h j j h g
            //          k l m m l k
            //          n o p p o n
            //          q r s s r q
            for (int x = 0; x < colPixels; ++x) {
                uint32_t* row = misspellBitmap[index]->getAddr32(0, x);
                row[0] = colors[index][x * 3];
                row[1] = colors[index][x * 3 + 1];
                row[2] = colors[index][x * 3 + 2];
                row[3] = colors[index][x * 3 + 2];
                row[4] = colors[index][x * 3 + 1];
                row[5] = colors[index][x * 3];
                row[6] = transparentColor;
                row[7] = transparentColor;
            }
        } else
            ASSERT_NOT_REACHED();
#else
        // We use a 2-pixel-high misspelling indicator because that seems to be
        // what WebKit is designed for, and how much room there is in a typical
        // page for it.
        const int rowPixels = 32 * deviceScaleFactor; // Must be multiple of 4 for pattern below.
        const int colPixels = 2 * deviceScaleFactor;
        misspellBitmap[index] = new SkBitmap;
        misspellBitmap[index]->setConfig(SkBitmap::kARGB_8888_Config, rowPixels, colPixels);
        misspellBitmap[index]->allocPixels();

        misspellBitmap[index]->eraseARGB(0, 0, 0, 0);
        if (deviceScaleFactor == 1)
            draw1xMarker(misspellBitmap[index], index);
        else if (deviceScaleFactor == 2)
            draw2xMarker(misspellBitmap[index], index);
        else
            ASSERT_NOT_REACHED();
#endif
    }

#if OS(DARWIN)
    SkScalar originX = WebCoreFloatToSkScalar(pt.x()) * deviceScaleFactor;
    SkScalar originY = WebCoreFloatToSkScalar(pt.y()) * deviceScaleFactor;

    // Make sure to draw only complete dots.
    int rowPixels = misspellBitmap[index]->width();
    float widthMod = fmodf(width * deviceScaleFactor, rowPixels);
    if (rowPixels - widthMod > deviceScaleFactor)
        width -= widthMod / deviceScaleFactor;
#else
    SkScalar originX = WebCoreFloatToSkScalar(pt.x());

    // Offset it vertically by 1 so that there's some space under the text.
    SkScalar originY = WebCoreFloatToSkScalar(pt.y()) + 1;
    originX *= deviceScaleFactor;
    originY *= deviceScaleFactor;
#endif

    // Make a shader for the bitmap with an origin of the box we'll draw. This
    // shader is refcounted and will have an initial refcount of 1.
    SkShader* shader = SkShader::CreateBitmapShader(
        *misspellBitmap[index], SkShader::kRepeat_TileMode,
        SkShader::kRepeat_TileMode);
    SkMatrix matrix;
    matrix.setTranslate(originX, originY);
    shader->setLocalMatrix(matrix);

    // Assign the shader to the paint & release our reference. The paint will
    // now own the shader and the shader will be destroyed when the paint goes
    // out of scope.
    SkPaint paint;
    paint.setShader(shader)->unref();

    SkRect rect;
    rect.set(originX, originY, originX + WebCoreFloatToSkScalar(width) * deviceScaleFactor, originY + SkIntToScalar(misspellBitmap[index]->height()));

    if (deviceScaleFactor == 2) {
        platformContext()->save();
        platformContext()->scale(SK_ScalarHalf, SK_ScalarHalf);
    }
    drawRect(rect, paint);
    if (deviceScaleFactor == 2)
        platformContext()->restore();
}

void GraphicsContext::drawLineForText(const FloatPoint& pt, float width, bool printing)
{
    if (paintingDisabled())
        return;

    if (width <= 0)
        return;

    int thickness = SkMax32(static_cast<int>(strokeThickness()), 1);
    SkRect r;
    r.fLeft = WebCoreFloatToSkScalar(pt.x());
    // Avoid anti-aliasing lines. Currently, these are always horizontal.
    // Round to nearest pixel to match text and other content.
    r.fTop = WebCoreFloatToSkScalar(floorf(pt.y() + 0.5f));
    r.fRight = r.fLeft + WebCoreFloatToSkScalar(width);
    r.fBottom = r.fTop + SkIntToScalar(thickness);

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    // Text lines are drawn using the stroke color.
    paint.setColor(platformContext()->effectiveStrokeColor());
    drawRect(r, paint);
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    ASSERT(!rect.isEmpty());
    if (rect.isEmpty())
        return;

    platformContext()->drawRect(rect);
}

void GraphicsContext::drawText(const Font& font, const TextRun& run, const FloatPoint& point, int from, int to)
{
    if (paintingDisabled())
        return;

    font.drawText(this, run, point, from, to);
}

void GraphicsContext::drawEmphasisMarks(const Font& font, const TextRun& run, const AtomicString& mark, const FloatPoint& point, int from, int to)
{
    if (paintingDisabled())
        return;

    font.drawEmphasisMarks(this, run, mark, point, from, to);
}

void GraphicsContext::drawBidiText(const Font& font, const TextRun& run, const FloatPoint& point, Font::CustomFontNotReadyAction customFontNotReadyAction)
{
    if (paintingDisabled())
        return;

    BidiResolver<TextRunIterator, BidiCharacterRun> bidiResolver;
    bidiResolver.setStatus(BidiStatus(run.direction(), run.directionalOverride()));
    bidiResolver.setPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));

    // FIXME: This ownership should be reversed. We should pass BidiRunList
    // to BidiResolver in createBidiRunsForLine.
    BidiRunList<BidiCharacterRun>& bidiRuns = bidiResolver.runs();
    bidiResolver.createBidiRunsForLine(TextRunIterator(&run, run.length()));
    if (!bidiRuns.runCount())
        return;

    FloatPoint currPoint = point;
    BidiCharacterRun* bidiRun = bidiRuns.firstRun();
    while (bidiRun) {
        TextRun subrun = run.subRun(bidiRun->start(), bidiRun->stop() - bidiRun->start());
        bool isRTL = bidiRun->level() % 2;
        subrun.setDirection(isRTL ? RTL : LTR);
        subrun.setDirectionalOverride(bidiRun->dirOverride(false));

        font.drawText(this, subrun, currPoint, 0, -1, customFontNotReadyAction);

        bidiRun = bidiRun->next();
        // FIXME: Have Font::drawText return the width of what it drew so that we don't have to re-measure here.
        if (bidiRun)
            currPoint.move(font.width(subrun), 0);
    }

    bidiRuns.deleteRuns();
}

void GraphicsContext::drawHighlightForText(const Font& font, const TextRun& run, const FloatPoint& point, int h, const Color& backgroundColor, ColorSpace colorSpace, int from, int to)
{
    if (paintingDisabled())
        return;

    fillRect(font.selectionRectForText(run, point, h, from, to), backgroundColor, colorSpace);
}

void GraphicsContext::drawImage(Image* image, ColorSpace styleColorSpace, const IntPoint& p, CompositeOperator op, RespectImageOrientationEnum shouldRespectImageOrientation)
{
    if (!image)
        return;
    drawImage(image, styleColorSpace, FloatRect(IntRect(p, image->size())), FloatRect(FloatPoint(), FloatSize(image->size())), op, shouldRespectImageOrientation);
}

void GraphicsContext::drawImage(Image* image, ColorSpace styleColorSpace, const IntRect& r, CompositeOperator op, RespectImageOrientationEnum shouldRespectImageOrientation, bool useLowQualityScale)
{
    if (!image)
        return;
    drawImage(image, styleColorSpace, FloatRect(r), FloatRect(FloatPoint(), FloatSize(image->size())), op, shouldRespectImageOrientation, useLowQualityScale);
}

void GraphicsContext::drawImage(Image* image, ColorSpace styleColorSpace, const IntPoint& dest, const IntRect& srcRect, CompositeOperator op, RespectImageOrientationEnum shouldRespectImageOrientation)
{
    drawImage(image, styleColorSpace, FloatRect(IntRect(dest, srcRect.size())), FloatRect(srcRect), op, shouldRespectImageOrientation);
}

void GraphicsContext::drawImage(Image* image, ColorSpace styleColorSpace, const FloatRect& dest, const FloatRect& src, CompositeOperator op, RespectImageOrientationEnum shouldRespectImageOrientation, bool useLowQualityScale)
{
    drawImage(image, styleColorSpace, dest, src, op, BlendModeNormal, shouldRespectImageOrientation, useLowQualityScale);
}

void GraphicsContext::drawImage(Image* image, ColorSpace styleColorSpace, const FloatRect& dest)
{
    if (!image)
        return;
    drawImage(image, styleColorSpace, dest, FloatRect(IntRect(IntPoint(), image->size())));
}

void GraphicsContext::drawImage(Image* image, ColorSpace styleColorSpace, const FloatRect& dest, const FloatRect& src, CompositeOperator op, BlendMode blendMode, RespectImageOrientationEnum shouldRespectImageOrientation, bool useLowQualityScale)
{    if (paintingDisabled() || !image)
        return;

    InterpolationQuality previousInterpolationQuality = InterpolationDefault;

    if (useLowQualityScale) {
        previousInterpolationQuality = imageInterpolationQuality();
        setImageInterpolationQuality(InterpolationLow);
    }

    image->draw(this, dest, src, styleColorSpace, op, blendMode, shouldRespectImageOrientation);

    if (useLowQualityScale)
        setImageInterpolationQuality(previousInterpolationQuality);
}

void GraphicsContext::drawTiledImage(Image* image, ColorSpace styleColorSpace, const IntRect& destRect, const IntPoint& srcPoint, const IntSize& tileSize, CompositeOperator op, bool useLowQualityScale, BlendMode blendMode)
{
    if (paintingDisabled() || !image)
        return;

    if (useLowQualityScale) {
        InterpolationQuality previousInterpolationQuality = imageInterpolationQuality();
        setImageInterpolationQuality(InterpolationLow);
        image->drawTiled(this, destRect, srcPoint, tileSize, styleColorSpace, op, blendMode);
        setImageInterpolationQuality(previousInterpolationQuality);
    } else
        image->drawTiled(this, destRect, srcPoint, tileSize, styleColorSpace, op, blendMode);
}

void GraphicsContext::drawTiledImage(Image* image, ColorSpace styleColorSpace, const IntRect& dest, const IntRect& srcRect,
    const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, CompositeOperator op, bool useLowQualityScale)
{
    if (paintingDisabled() || !image)
        return;

    if (hRule == Image::StretchTile && vRule == Image::StretchTile) {
        // Just do a scale.
        drawImage(image, styleColorSpace, dest, srcRect, op);
        return;
    }

    if (useLowQualityScale) {
        InterpolationQuality previousInterpolationQuality = imageInterpolationQuality();
        setImageInterpolationQuality(InterpolationLow);
        image->drawTiled(this, dest, srcRect, tileScaleFactor, hRule, vRule, styleColorSpace, op);
        setImageInterpolationQuality(previousInterpolationQuality);
    } else
        image->drawTiled(this, dest, srcRect, tileScaleFactor, hRule, vRule, styleColorSpace, op);
}

void GraphicsContext::drawImageBuffer(ImageBuffer* image, ColorSpace styleColorSpace, const IntPoint& p, CompositeOperator op, BlendMode blendMode)
{
    if (!image)
        return;
    drawImageBuffer(image, styleColorSpace, FloatRect(IntRect(p, image->logicalSize())), FloatRect(FloatPoint(), FloatSize(image->logicalSize())), op, blendMode);
}

void GraphicsContext::drawImageBuffer(ImageBuffer* image, ColorSpace styleColorSpace, const IntRect& r, CompositeOperator op, BlendMode blendMode, bool useLowQualityScale)
{
    if (!image)
        return;
    drawImageBuffer(image, styleColorSpace, FloatRect(r), FloatRect(FloatPoint(), FloatSize(image->logicalSize())), op, blendMode, useLowQualityScale);
}

void GraphicsContext::drawImageBuffer(ImageBuffer* image, ColorSpace styleColorSpace, const IntPoint& dest, const IntRect& srcRect, CompositeOperator op, BlendMode blendMode)
{
    drawImageBuffer(image, styleColorSpace, FloatRect(IntRect(dest, srcRect.size())), FloatRect(srcRect), op, blendMode);
}

void GraphicsContext::drawImageBuffer(ImageBuffer* image, ColorSpace styleColorSpace, const IntRect& dest, const IntRect& srcRect, CompositeOperator op, BlendMode blendMode, bool useLowQualityScale)
{
    drawImageBuffer(image, styleColorSpace, FloatRect(dest), FloatRect(srcRect), op, blendMode, useLowQualityScale);
}

void GraphicsContext::drawImageBuffer(ImageBuffer* image, ColorSpace styleColorSpace, const FloatRect& dest)
{
    if (!image)
        return;
    drawImageBuffer(image, styleColorSpace, dest, FloatRect(IntRect(IntPoint(), image->logicalSize())));
}

void GraphicsContext::drawImageBuffer(ImageBuffer* image, ColorSpace styleColorSpace, const FloatRect& dest, const FloatRect& src, CompositeOperator op, BlendMode blendMode, bool useLowQualityScale)
{
    if (paintingDisabled() || !image)
        return;

    if (useLowQualityScale) {
        InterpolationQuality previousInterpolationQuality = imageInterpolationQuality();
        setImageInterpolationQuality(InterpolationLow);
        image->draw(this, styleColorSpace, dest, src, op, blendMode, useLowQualityScale);
        setImageInterpolationQuality(previousInterpolationQuality);
    } else
        image->draw(this, styleColorSpace, dest, src, op, blendMode, useLowQualityScale);
}

void GraphicsContext::writePixels(const SkBitmap& bitmap, int x, int y, SkCanvas::Config8888 config8888)
{
    platformContext()->canvas()->writePixels(bitmap, x, y, config8888);

    if (m_trackOpaqueRegion) {
        SkRect rect = SkRect::MakeXYWH(x, y, bitmap.width(), bitmap.height());
        SkPaint paint;

        paint.setXfermodeMode(SkXfermode::kSrc_Mode);
        m_opaqueRegion.didDrawRect(this, rect, paint, &bitmap);
    }
}

void GraphicsContext::drawBitmap(const SkBitmap& bitmap, SkScalar left, SkScalar top,
    const SkPaint* paint)
{
    platformContext()->canvas()->drawBitmap(bitmap, left, top, paint);

    if (m_trackOpaqueRegion) {
        SkRect rect = SkRect::MakeXYWH(left, top, bitmap.width(), bitmap.height());
        m_opaqueRegion.didDrawRect(this, rect, *paint, &bitmap);
    }
}

void GraphicsContext::drawBitmapRect(const SkBitmap& bitmap, const SkIRect* isrc,
    const SkRect& dst, const SkPaint* paint)
{
    platformContext()->canvas()->drawBitmapRect(bitmap, isrc, dst, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawRect(this, dst, *paint, &bitmap);
}

void GraphicsContext::drawOval(const SkRect& oval, const SkPaint& paint)
{
    platformContext()->canvas()->drawOval(oval, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawBounded(this, oval, paint);
}

void GraphicsContext::drawPath(const SkPath& path, const SkPaint& paint)
{
    platformContext()->canvas()->drawPath(path, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawPath(this, path, paint);
}

void GraphicsContext::drawRect(const SkRect& rect, const SkPaint& paint)
{
    platformContext()->canvas()->drawRect(rect, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawRect(this, rect, paint, 0);
}

void GraphicsContext::didDrawRect(const SkRect& rect, const SkPaint& paint, const SkBitmap* bitmap)
{
    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawRect(this, rect, paint, bitmap);
}

void GraphicsContext::drawPosText(const void* text, size_t byteLength,
    const SkPoint pos[], const SkPaint& paint)
{
    platformContext()->canvas()->drawPosText(text, byteLength, pos, paint);

    // FIXME: compute bounds for positioned text.
    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawUnbounded(this, paint, OpaqueRegionSkia::FillOrStroke);
}

void GraphicsContext::drawPosTextH(const void* text, size_t byteLength,
    const SkScalar xpos[], SkScalar constY, const SkPaint& paint)
{
    platformContext()->canvas()->drawPosTextH(text, byteLength, xpos, constY, paint);

    // FIXME: compute bounds for positioned text.
    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawUnbounded(this, paint, OpaqueRegionSkia::FillOrStroke);
}

void GraphicsContext::drawTextOnPath(const void* text, size_t byteLength,
    const SkPath& path, const SkMatrix* matrix, const SkPaint& paint)
{
    platformContext()->canvas()->drawTextOnPath(text, byteLength, path, matrix, paint);

    // FIXME: compute bounds for positioned text.
    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawUnbounded(this, paint, OpaqueRegionSkia::FillOrStroke);
}

void GraphicsContext::fillPath(const Path& pathToFill)
{
    if (paintingDisabled() || pathToFill.isEmpty())
        return;

    // Use const_cast and temporarily modify the fill type instead of copying the path.
    SkPath& path = const_cast<SkPath&>(pathToFill.skPath());
    SkPath::FillType previousFillType = path.getFillType();

    SkPath::FillType temporaryFillType = m_state.fillRule == RULE_EVENODD ? SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType;
    path.setFillType(temporaryFillType);

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    drawPath(path, paint);

    path.setFillType(previousFillType);
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    drawRect(r, paint);
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;
    SkPaint paint;
    platformContext()->setupPaintCommon(&paint);
    paint.setColor(color.rgb());
    drawRect(r, paint);
}

void GraphicsContext::fillRoundedRect(const IntRect& rect,
                                      const IntSize& topLeft,
                                      const IntSize& topRight,
                                      const IntSize& bottomLeft,
                                      const IntSize& bottomRight,
                                      const Color& color,
                                      ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    if (topLeft.width() + topRight.width() > rect.width()
            || bottomLeft.width() + bottomRight.width() > rect.width()
            || topLeft.height() + bottomLeft.height() > rect.height()
            || topRight.height() + bottomRight.height() > rect.height()) {
        // Not all the radii fit, return a rect. This matches the behavior of
        // Path::createRoundedRectangle. Without this we attempt to draw a round
        // shadow for a square box.
        fillRect(rect, color, colorSpace);
        return;
    }

    SkVector radii[4];
    setRadii(radii, topLeft, topRight, bottomRight, bottomLeft);

    SkRRect rr;
    rr.setRectRadii(rect, radii);

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    paint.setColor(color.rgb());

    platformContext()->canvas()->drawRRect(rr, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawBounded(this, rr.getBounds(), paint);
}

void GraphicsContext::fillEllipse(const FloatRect& ellipse)
{
    if (paintingDisabled())
        return;

    SkRect rect = ellipse;
    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    drawOval(rect, paint);
}

void GraphicsContext::strokePath(const Path& pathToStroke)
{
    if (paintingDisabled() || pathToStroke.isEmpty())
        return;

    const SkPath& path = pathToStroke.skPath();
    SkPaint paint;
    platformContext()->setupPaintForStroking(&paint, 0, 0);
    drawPath(path, paint);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (paintingDisabled())
        return;

    SkPaint paint;
    platformContext()->setupPaintForStroking(&paint, 0, 0);
    paint.setStrokeWidth(WebCoreFloatToSkScalar(lineWidth));
    // strokerect has special rules for CSS when the rect is degenerate:
    // if width==0 && height==0, do nothing
    // if width==0 || height==0, then just draw line for the other dimension
    SkRect r(rect);
    bool validW = r.width() > 0;
    bool validH = r.height() > 0;
    if (validW && validH) {
        drawRect(r, paint);
    } else if (validW || validH) {
        // we are expected to respect the lineJoin, so we can't just call
        // drawLine -- we have to create a path that doubles back on itself.
        SkPath path;
        path.moveTo(r.fLeft, r.fTop);
        path.lineTo(r.fRight, r.fBottom);
        path.close();
        drawPath(path, paint);
    }
}

void GraphicsContext::strokeEllipse(const FloatRect& ellipse)
{
    if (paintingDisabled())
        return;

    SkRect rect(ellipse);
    SkPaint paint;
    platformContext()->setupPaintForStroking(&paint, 0, 0);
    drawOval(rect, paint);
}

void GraphicsContext::clip(const IntRect& rect)
{
    clip(FloatRect(rect));
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    platformContext()->clipRect(rect);
}

void GraphicsContext::clip(const Path& path, WindRule clipRule)
{
    if (paintingDisabled() || path.isEmpty())
        return;

    clipPath(path, clipRule);
}

void GraphicsContext::clipRoundedRect(const RoundedRect& rect)
{
    if (paintingDisabled())
        return;

    SkVector radii[4];
    RoundedRect::Radii wkRadii = rect.radii();
    setRadii(radii, wkRadii.topLeft(), wkRadii.topRight(), wkRadii.bottomRight(), wkRadii.bottomLeft());

    SkRRect r;
    r.setRectRadii(rect.rect(), radii);

    platformContext()->clipRRect(r, PlatformContextSkia::AntiAliased);
}

void GraphicsContext::clipOut(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    platformContext()->clipRect(rect, PlatformContextSkia::NotAntiAliased, SkRegion::kDifference_Op);
}

void GraphicsContext::clipOut(const Path& pathToClip)
{
    if (paintingDisabled())
        return;

    // Use const_cast and temporarily toggle the inverse fill type instead of copying the path.
    SkPath& path = const_cast<SkPath&>(pathToClip.skPath());
    path.toggleInverseFillType();
    platformContext()->clipPath(path, PlatformContextSkia::AntiAliased);
    path.toggleInverseFillType();
}

void GraphicsContext::clipPath(const Path& pathToClip, WindRule clipRule)
{
    if (paintingDisabled())
        return;

    // Use const_cast and temporarily modify the fill type instead of copying the path.
    SkPath& path = const_cast<SkPath&>(pathToClip.skPath());
    SkPath::FillType previousFillType = path.getFillType();

    SkPath::FillType temporaryFillType = clipRule == RULE_EVENODD ? SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType;
    path.setFillType(temporaryFillType);
    platformContext()->clipPath(path, PlatformContextSkia::AntiAliased);

    path.setFillType(previousFillType);
}

void GraphicsContext::clipConvexPolygon(size_t numPoints, const FloatPoint* points, bool antialiased)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;

    SkPath path;
    setPathFromConvexPoints(&path, numPoints, points);
    platformContext()->clipPath(path, antialiased
        ? PlatformContextSkia::AntiAliased
        : PlatformContextSkia::NotAntiAliased);
}

void GraphicsContext::clipOutRoundedRect(const RoundedRect& rect)
{
    if (paintingDisabled())
        return;

    if (!rect.isRounded()) {
        clipOut(rect.rect());
        return;
    }

    Path path;
    path.addRoundedRect(rect);
    clipOut(path);
}

void GraphicsContext::clipToImageBuffer(ImageBuffer* buffer, const FloatRect& rect)
{
    if (paintingDisabled())
        return;
    buffer->clip(this, rect);
}

void GraphicsContext::canvasClip(const Path& pathToClip, WindRule clipRule)
{
    if (paintingDisabled())
        return;

    // Use const_cast and temporarily modify the fill type instead of copying the path.
    SkPath& path = const_cast<SkPath&>(pathToClip.skPath());
    SkPath::FillType previousFillType = path.getFillType();

    SkPath::FillType temporaryFillType = clipRule == RULE_EVENODD ? SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType;
    path.setFillType(temporaryFillType);
    platformContext()->clipPath(path);

    path.setFillType(previousFillType);
}

void GraphicsContext::rotate(float angleInRadians)
{
    if (paintingDisabled())
        return;

    platformContext()->rotate(WebCoreFloatToSkScalar(angleInRadians * (180.0f / 3.14159265f)));
}

void GraphicsContext::translate(float w, float h)
{
    if (paintingDisabled())
        return;

    platformContext()->translate(WebCoreFloatToSkScalar(w), WebCoreFloatToSkScalar(h));
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;

    platformContext()->scale(WebCoreFloatToSkScalar(size.width()),
        WebCoreFloatToSkScalar(size.height()));
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
    if (paintingDisabled())
        return;

    SkAutoDataUnref url(SkData::NewWithCString(link.string().utf8().data()));
    SkAnnotateRectWithURL(platformContext()->canvas(), destRect, url.get());
}

void GraphicsContext::setURLFragmentForRect(const String& destName, const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkAutoDataUnref skDestName(SkData::NewWithCString(destName.utf8().data()));
    SkAnnotateLinkToDestination(platformContext()->canvas(), rect, skDestName.get());
}

void GraphicsContext::addURLTargetAtPoint(const String& name, const IntPoint& pos)
{
    if (paintingDisabled())
        return;

    SkAutoDataUnref nameData(SkData::NewWithCString(name.utf8().data()));
    SkAnnotateNamedDestination(platformContext()->canvas(),
        SkPoint::Make(pos.x(), pos.y()), nameData);
}

void GraphicsContext::concatCTM(const AffineTransform& affine)
{
    if (paintingDisabled())
        return;

    platformContext()->concat(affine);
}

void GraphicsContext::setCTM(const AffineTransform& affine)
{
    if (paintingDisabled())
        return;

    platformContext()->setMatrix(affine);
}

AffineTransform GraphicsContext::getCTM(IncludeDeviceScale) const
{
    if (paintingDisabled())
        return AffineTransform();

    const SkMatrix& m = platformContext()->getTotalMatrix();
    return AffineTransform(SkScalarToDouble(m.getScaleX()),
                           SkScalarToDouble(m.getSkewY()),
                           SkScalarToDouble(m.getSkewX()),
                           SkScalarToDouble(m.getScaleY()),
                           SkScalarToDouble(m.getTranslateX()),
                           SkScalarToDouble(m.getTranslateY()));
}

TextDrawingModeFlags GraphicsContext::textDrawingMode() const
{
    return m_state.textDrawingMode;
}

void GraphicsContext::setTextDrawingMode(TextDrawingModeFlags mode)
{
    m_state.textDrawingMode = mode;
    if (paintingDisabled())
        return;

    platformContext()->setTextDrawingMode(mode);
}

void GraphicsContext::fillRect(const FloatRect& rect, Generator& generator)
{
    if (paintingDisabled())
        return;
    generator.fill(this, rect);
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, ColorSpace styleColorSpace, CompositeOperator op)
{
    if (paintingDisabled())
        return;

    CompositeOperator previousOperator = compositeOperation();
    setCompositeOperation(op);
    fillRect(rect, color, styleColorSpace);
    setCompositeOperation(previousOperator);
}

void GraphicsContext::fillRoundedRect(const RoundedRect& rect, const Color& color, ColorSpace colorSpace)
{
    if (rect.isRounded())
        fillRoundedRect(rect.rect(), rect.radii().topLeft(), rect.radii().topRight(), rect.radii().bottomLeft(), rect.radii().bottomRight(), color, colorSpace);
    else
        fillRect(rect.rect(), color, colorSpace);
}

void GraphicsContext::fillRectWithRoundedHole(const IntRect& rect, const RoundedRect& roundedHoleRect, const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    Path path;
    path.addRect(rect);

    if (!roundedHoleRect.radii().isZero())
        path.addRoundedRect(roundedHoleRect);
    else
        path.addRect(roundedHoleRect.rect());

    WindRule oldFillRule = fillRule();
    Color oldFillColor = fillColor();
    ColorSpace oldFillColorSpace = fillColorSpace();
    
    setFillRule(RULE_EVENODD);
    setFillColor(color, colorSpace);

    fillPath(path);
    
    setFillRule(oldFillRule);
    setFillColor(oldFillColor, oldFillColorSpace);
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;
    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    paint.setXfermodeMode(SkXfermode::kClear_Mode);
    drawRect(r, paint);
}

void GraphicsContext::setCompositeOperation(CompositeOperator compositeOperation, BlendMode blendMode)
{
    m_state.compositeOperator = compositeOperation;
    m_state.blendMode = blendMode;

    if (paintingDisabled())
        return;

    platformContext()->setXfermodeMode(WebCoreCompositeToSkiaComposite(compositeOperation));
}

CompositeOperator GraphicsContext::compositeOperation() const
{
    return m_state.compositeOperator;
}

BlendMode GraphicsContext::blendModeOperation() const
{
    return m_state.blendMode;
}

void GraphicsContext::adjustLineToPixelBoundaries(FloatPoint& p1, FloatPoint& p2, float strokeWidth, StrokeStyle penStyle)
{
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, WebKit will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (penStyle == DottedStroke || penStyle == DashedStroke) {
        if (p1.x() == p2.x()) {
            p1.setY(p1.y() + strokeWidth);
            p2.setY(p2.y() - strokeWidth);
        } else {
            p1.setX(p1.x() + strokeWidth);
            p2.setX(p2.x() - strokeWidth);
        }
    }

    if (static_cast<int>(strokeWidth) % 2) { //odd
        if (p1.x() == p2.x()) {
            // We're a vertical line.  Adjust our x.
            p1.setX(p1.x() + 0.5f);
            p2.setX(p2.x() + 0.5f);
        } else {
            // We're a horizontal line. Adjust our y.
            p1.setY(p1.y() + 0.5f);
            p2.setY(p2.y() + 0.5f);
        }
    }
}

static bool scalesMatch(AffineTransform a, AffineTransform b)
{
    return a.xScale() == b.xScale() && a.yScale() == b.yScale();
}

PassOwnPtr<ImageBuffer> GraphicsContext::createCompatibleBuffer(const IntSize& size, bool hasAlpha) const
{
    // Make the buffer larger if the context's transform is scaling it so we need a higher
    // resolution than one pixel per unit. Also set up a corresponding scale factor on the
    // graphics context.

    AffineTransform transform = getCTM(DefinitelyIncludeDeviceScale);
    IntSize scaledSize(static_cast<int>(ceil(size.width() * transform.xScale())), static_cast<int>(ceil(size.height() * transform.yScale())));

    OwnPtr<ImageBuffer> buffer = ImageBuffer::createCompatibleBuffer(scaledSize, 1, ColorSpaceDeviceRGB, this, hasAlpha);
    if (!buffer)
        return nullptr;

    buffer->context()->scale(FloatSize(static_cast<float>(scaledSize.width()) / size.width(),
        static_cast<float>(scaledSize.height()) / size.height()));

    return buffer.release();
}

bool GraphicsContext::isCompatibleWithBuffer(ImageBuffer* buffer) const
{
    GraphicsContext* bufferContext = buffer->context();

    return scalesMatch(getCTM(), bufferContext->getCTM()) && m_accelerated == bufferContext->isAccelerated();
}

void GraphicsContext::applyDeviceScaleFactor(float deviceScaleFactor)
{
    scale(FloatSize(deviceScaleFactor, deviceScaleFactor));
}

void GraphicsContext::addCornerArc(SkPath* path, const SkRect& rect, const IntSize& size, int startAngle)
{
    SkIRect ir;
    int rx = SkMin32(SkScalarRound(rect.width()), size.width());
    int ry = SkMin32(SkScalarRound(rect.height()), size.height());

    ir.set(-rx, -ry, rx, ry);
    switch (startAngle) {
    case 0:
        ir.offset(rect.fRight - ir.fRight, rect.fBottom - ir.fBottom);
        break;
    case 90:
        ir.offset(rect.fLeft - ir.fLeft, rect.fBottom - ir.fBottom);
        break;
    case 180:
        ir.offset(rect.fLeft - ir.fLeft, rect.fTop - ir.fTop);
        break;
    case 270:
        ir.offset(rect.fRight - ir.fRight, rect.fTop - ir.fTop);
        break;
    default:
        ASSERT(0);
    }

    SkRect r;
    r.set(ir);
    path->arcTo(r, SkIntToScalar(startAngle), SkIntToScalar(90), false);
}

void GraphicsContext::setPathFromConvexPoints(SkPath* path, size_t numPoints, const FloatPoint* points)
{
    path->incReserve(numPoints);
    path->moveTo(WebCoreFloatToSkScalar(points[0].x()),
                 WebCoreFloatToSkScalar(points[0].y()));
    for (size_t i = 1; i < numPoints; ++i) {
        path->lineTo(WebCoreFloatToSkScalar(points[i].x()),
                     WebCoreFloatToSkScalar(points[i].y()));
    }

    /*  The code used to just blindly call this
            path->setIsConvex(true);
        But webkit can sometimes send us non-convex 4-point values, so we mark the path's
        convexity as unknown, so it will get computed by skia at draw time.
        See crbug.com 108605
    */
    SkPath::Convexity convexity = SkPath::kConvex_Convexity;
    if (numPoints == 4)
        convexity = SkPath::kUnknown_Convexity;
    path->setConvexity(convexity);
}

void GraphicsContext::drawOuterPath(const SkPath& path, SkPaint& paint, int width)
{
#if OS(DARWIN)
    paint.setAlpha(64);
    paint.setStrokeWidth(width);
    paint.setPathEffect(new SkCornerPathEffect((width - 1) * 0.5f))->unref();
#else
    paint.setStrokeWidth(1);
    paint.setPathEffect(new SkCornerPathEffect(1))->unref();
#endif
    drawPath(path, paint);
}

void GraphicsContext::drawInnerPath(const SkPath& path, SkPaint& paint, int width)
{
#if OS(DARWIN)
    paint.setAlpha(128);
    paint.setStrokeWidth(width * 0.5f);
    drawPath(path, paint);
#endif
}

void GraphicsContext::setRadii(SkVector* radii, IntSize topLeft, IntSize topRight, IntSize bottomRight, IntSize bottomLeft)
{
    radii[SkRRect::kUpperLeft_Corner].set(SkIntToScalar(topLeft.width()),
        SkIntToScalar(topLeft.height()));
    radii[SkRRect::kUpperRight_Corner].set(SkIntToScalar(topRight.width()),
        SkIntToScalar(topRight.height()));
    radii[SkRRect::kLowerRight_Corner].set(SkIntToScalar(bottomRight.width()),
        SkIntToScalar(bottomRight.height()));
    radii[SkRRect::kLowerLeft_Corner].set(SkIntToScalar(bottomLeft.width()),
        SkIntToScalar(bottomLeft.height()));
}

#if OS(DARWIN)
CGColorSpaceRef deviceRGBColorSpaceRef()
{
    static CGColorSpaceRef deviceSpace = CGColorSpaceCreateDeviceRGB();
    return deviceSpace;
}
#else
void GraphicsContext::draw2xMarker(SkBitmap* bitmap, int index)
{
    const SkPMColor lineColor = lineColors(index);
    const SkPMColor antiColor1 = antiColors1(index);
    const SkPMColor antiColor2 = antiColors2(index);

    uint32_t* row1 = bitmap->getAddr32(0, 0);
    uint32_t* row2 = bitmap->getAddr32(0, 1);
    uint32_t* row3 = bitmap->getAddr32(0, 2);
    uint32_t* row4 = bitmap->getAddr32(0, 3);

    // Pattern: X0o   o0X0o   o0
    //          XX0o o0XXX0o o0X
    //           o0XXX0o o0XXX0o
    //            o0X0o   o0X0o
    const SkPMColor row1Color[] = { lineColor, antiColor1, antiColor2, 0,          0,         0,          antiColor2, antiColor1 };
    const SkPMColor row2Color[] = { lineColor, lineColor,  antiColor1, antiColor2, 0,         antiColor2, antiColor1, lineColor };
    const SkPMColor row3Color[] = { 0,         antiColor2, antiColor1, lineColor,  lineColor, lineColor,  antiColor1, antiColor2 };
    const SkPMColor row4Color[] = { 0,         0,          antiColor2, antiColor1, lineColor, antiColor1, antiColor2, 0 };

    for (int x = 0; x < bitmap->width() + 8; x += 8) {
        int count = std::min(bitmap->width() - x, 8);
        if (count > 0) {
            memcpy(row1 + x, row1Color, count * sizeof(SkPMColor));
            memcpy(row2 + x, row2Color, count * sizeof(SkPMColor));
            memcpy(row3 + x, row3Color, count * sizeof(SkPMColor));
            memcpy(row4 + x, row4Color, count * sizeof(SkPMColor));
        }
    }
}

void GraphicsContext::draw1xMarker(SkBitmap* bitmap, int index)
{
    const uint32_t lineColor = lineColors(index);
    const uint32_t antiColor = antiColors2(index);

    // Pattern: X o   o X o   o X
    //            o X o   o X o
    uint32_t* row1 = bitmap->getAddr32(0, 0);
    uint32_t* row2 = bitmap->getAddr32(0, 1);
    for (int x = 0; x < bitmap->width(); x++) {
        switch (x % 4) {
        case 0:
            row1[x] = lineColor;
            break;
        case 1:
            row1[x] = antiColor;
            row2[x] = antiColor;
            break;
        case 2:
            row2[x] = lineColor;
            break;
        case 3:
            row1[x] = antiColor;
            row2[x] = antiColor;
            break;
        }
    }
}

const SkPMColor GraphicsContext::lineColors(int index)
{
    static const SkPMColor colors[] = {
        SkPreMultiplyARGB(0xFF, 0xFF, 0x00, 0x00), // Opaque red.
        SkPreMultiplyARGB(0xFF, 0xC0, 0xC0, 0xC0) // Opaque gray.
    };

    return colors[index];
}

const SkPMColor GraphicsContext::antiColors1(int index)
{
    static const SkPMColor colors[] = {
        SkPreMultiplyARGB(0xB0, 0xFF, 0x00, 0x00), // Semitransparent red.
        SkPreMultiplyARGB(0xB0, 0xC0, 0xC0, 0xC0)  // Semitransparent gray.
    };

    return colors[index];
}

const SkPMColor GraphicsContext::antiColors2(int index)
{
    static const SkPMColor colors[] = {
        SkPreMultiplyARGB(0x60, 0xFF, 0x00, 0x00), // More transparent red
        SkPreMultiplyARGB(0x60, 0xC0, 0xC0, 0xC0)  // More transparent gray
    };

    return colors[index];
}
#endif

}
