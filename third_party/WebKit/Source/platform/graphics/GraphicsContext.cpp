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
#include "platform/graphics/GraphicsContext.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/Gradient.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/weborigin/KURL.h"
#include "third_party/skia/include/core/SkAnnotation.h"
#include "third_party/skia/include/core/SkClipStack.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "third_party/skia/include/effects/SkCornerPathEffect.h"
#include "third_party/skia/include/effects/SkDropShadowImageFilter.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"
#include "third_party/skia/include/effects/SkMatrixImageFilter.h"
#include "third_party/skia/include/effects/SkPictureImageFilter.h"
#include "third_party/skia/include/gpu/GrRenderTarget.h"
#include "third_party/skia/include/gpu/GrTexture.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"
#include "wtf/Assertions.h"
#include "wtf/MathExtras.h"

namespace blink {

class GraphicsContext::RecordingState {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(RecordingState);
public:
    static PassOwnPtr<RecordingState> Create(SkCanvas* canvas, const SkMatrix& matrix)
    {
        return adoptPtr(new RecordingState(canvas, matrix));
    }

    SkPictureRecorder& recorder() { return m_recorder; }
    SkCanvas* canvas() const { return m_savedCanvas; }
    const SkMatrix& matrix() const { return m_savedMatrix; }

private:
    explicit RecordingState(SkCanvas* canvas, const SkMatrix& matrix)
        : m_savedCanvas(canvas)
        , m_savedMatrix(matrix)
    { }

    SkPictureRecorder m_recorder;
    SkCanvas* m_savedCanvas;
    const SkMatrix m_savedMatrix;
};

GraphicsContext::GraphicsContext(SkCanvas* canvas, DisplayItemList* displayItemList, DisabledMode disableContextOrPainting)
    : m_canvas(canvas)
    , m_displayItemList(displayItemList)
    , m_clipRecorderStack(0)
    , m_paintStateStack()
    , m_paintStateIndex(0)
    , m_annotationMode(0)
    , m_layerCount(0)
#if ENABLE(ASSERT)
    , m_annotationCount(0)
    , m_disableDestructionChecks(false)
    , m_inDrawingRecorder(false)
#endif
    , m_disabledState(disableContextOrPainting)
    , m_deviceScaleFactor(1.0f)
    , m_trackTextRegion(false)
    , m_accelerated(false)
    , m_printing(false)
    , m_antialiasHairlineImages(false)
{
    // FIXME: Do some tests to determine how many states are typically used, and allocate
    // several here.
    m_paintStateStack.append(GraphicsContextState::create());
    m_paintState = m_paintStateStack.last().get();

    if (contextDisabled()) {
        DEFINE_STATIC_LOCAL(RefPtr<SkCanvas>, nullCanvas, (adoptRef(SkCreateNullCanvas())));
        m_canvas = nullCanvas.get();
    }
}

GraphicsContext::~GraphicsContext()
{
#if ENABLE(ASSERT)
    if (!m_disableDestructionChecks) {
        ASSERT(!m_paintStateIndex);
        ASSERT(!m_paintState->saveCount());
        ASSERT(!m_annotationCount);
        ASSERT(!m_layerCount);
        ASSERT(m_recordingStateStack.isEmpty());
        ASSERT(!saveCount());
    }
#endif
}

void GraphicsContext::resetCanvas(SkCanvas* canvas)
{
    m_canvas = canvas;
}

void GraphicsContext::save()
{
    if (contextDisabled())
        return;

    m_paintState->incrementSaveCount();

    ASSERT(m_canvas);
    m_canvas->save();
}

void GraphicsContext::restore()
{
    if (contextDisabled())
        return;

    if (!m_paintStateIndex && !m_paintState->saveCount()) {
        WTF_LOG_ERROR("ERROR void GraphicsContext::restore() stack is empty");
        return;
    }

    if (m_paintState->saveCount()) {
        m_paintState->decrementSaveCount();
    } else {
        m_paintStateIndex--;
        m_paintState = m_paintStateStack[m_paintStateIndex].get();
    }

    ASSERT(m_canvas);
    m_canvas->restore();
}

#if ENABLE(ASSERT)
unsigned GraphicsContext::saveCount() const
{
    // Each m_paintStateStack entry implies an additional save op
    // (on top of its own saveCount), except for the first frame.
    unsigned count = m_paintStateIndex;
    ASSERT(m_paintStateStack.size() > m_paintStateIndex);
    for (unsigned i = 0; i <= m_paintStateIndex; ++i)
        count += m_paintStateStack[i]->saveCount();

    return count;
}
#endif

void GraphicsContext::saveLayer(const SkRect* bounds, const SkPaint* paint)
{
    if (contextDisabled())
        return;

    ASSERT(m_canvas);

    m_canvas->saveLayer(bounds, paint);
}

void GraphicsContext::restoreLayer()
{
    if (contextDisabled())
        return;

    ASSERT(m_canvas);

    m_canvas->restore();
}

void GraphicsContext::beginAnnotation(const AnnotationList& annotations)
{
    if (contextDisabled())
        return;

    ASSERT(m_canvas);

    canvas()->beginCommentGroup("GraphicsContextAnnotation");

    AnnotationList::const_iterator end = annotations.end();
    for (AnnotationList::const_iterator it = annotations.begin(); it != end; ++it)
        canvas()->addComment(it->first, it->second.ascii().data());

#if ENABLE(ASSERT)
    ++m_annotationCount;
#endif
}

void GraphicsContext::endAnnotation()
{
    if (contextDisabled())
        return;

    ASSERT(m_canvas);
    ASSERT(m_annotationCount > 0);
    canvas()->endCommentGroup();

#if ENABLE(ASSERT)
    --m_annotationCount;
#endif
}

#if ENABLE(ASSERT)
void GraphicsContext::setInDrawingRecorder(bool val)
{
    // Nested drawing recorers are not allowed.
    ASSERT(!val || !m_inDrawingRecorder);
    m_inDrawingRecorder = val;
}
#endif

void GraphicsContext::setStrokePattern(PassRefPtr<Pattern> pattern, float alpha)
{
    if (contextDisabled())
        return;

    ASSERT(pattern);
    if (!pattern) {
        setStrokeColor(Color::black);
        return;
    }

    mutableState()->setStrokePattern(pattern, alpha);
}

void GraphicsContext::setStrokeGradient(PassRefPtr<Gradient> gradient, float alpha)
{
    if (contextDisabled())
        return;

    ASSERT(gradient);
    if (!gradient) {
        setStrokeColor(Color::black);
        return;
    }
    mutableState()->setStrokeGradient(gradient, alpha);
}

void GraphicsContext::setFillPattern(PassRefPtr<Pattern> pattern, float alpha)
{
    if (contextDisabled())
        return;

    ASSERT(pattern);
    if (!pattern) {
        setFillColor(Color::black);
        return;
    }

    mutableState()->setFillPattern(pattern, alpha);
}

void GraphicsContext::setFillGradient(PassRefPtr<Gradient> gradient, float alpha)
{
    if (contextDisabled())
        return;

    ASSERT(gradient);
    if (!gradient) {
        setFillColor(Color::black);
        return;
    }

    mutableState()->setFillGradient(gradient, alpha);
}

void GraphicsContext::setShadow(const FloatSize& offset, float blur, const Color& color,
    DrawLooperBuilder::ShadowTransformMode shadowTransformMode,
    DrawLooperBuilder::ShadowAlphaMode shadowAlphaMode, ShadowMode shadowMode)
{
    if (contextDisabled())
        return;

    OwnPtr<DrawLooperBuilder> drawLooperBuilder = DrawLooperBuilder::create();
    if (!color.alpha()) {
        if (shadowMode == DrawShadowOnly) {
            // shadow only, but there is no shadow: use an empty draw looper to disable rendering of the source primitive
            setDrawLooper(drawLooperBuilder.release());
            return;
        }
        clearShadow();
        return;
    }

    drawLooperBuilder->addShadow(offset, blur, color, shadowTransformMode, shadowAlphaMode);
    if (shadowMode == DrawShadowAndForeground) {
        drawLooperBuilder->addUnmodifiedContent();
    }
    setDrawLooper(drawLooperBuilder.release());

    if (shadowTransformMode == DrawLooperBuilder::ShadowIgnoresTransforms
        && shadowAlphaMode == DrawLooperBuilder::ShadowRespectsAlpha) {
        // This image filter will be used in place of the drawLooper created above but only for drawing non-opaque bitmaps;
        // see preparePaintForDrawRectToRect().
        SkColor skColor = color.rgb();
        // These constants are from RadiusToSigma() from DrawLooperBuilder.cpp.
        const SkScalar sigma = 0.288675f * blur + 0.5f;
        SkDropShadowImageFilter::ShadowMode dropShadowMode = shadowMode == DrawShadowAndForeground ? SkDropShadowImageFilter::kDrawShadowAndForeground_ShadowMode : SkDropShadowImageFilter::kDrawShadowOnly_ShadowMode;
        RefPtr<SkImageFilter> filter = adoptRef(SkDropShadowImageFilter::Create(offset.width(), offset.height(), sigma, sigma, skColor, dropShadowMode));
        setDropShadowImageFilter(filter);
    }
}

void GraphicsContext::setDrawLooper(PassOwnPtr<DrawLooperBuilder> drawLooperBuilder)
{
    if (contextDisabled())
        return;

    mutableState()->setDrawLooper(drawLooperBuilder->detachDrawLooper());
}

void GraphicsContext::clearDrawLooper()
{
    if (contextDisabled())
        return;

    mutableState()->clearDrawLooper();
}

void GraphicsContext::setDropShadowImageFilter(PassRefPtr<SkImageFilter> imageFilter)
{
    if (contextDisabled())
        return;

    mutableState()->setDropShadowImageFilter(imageFilter);
}

void GraphicsContext::clearDropShadowImageFilter()
{
    if (contextDisabled())
        return;

    mutableState()->clearDropShadowImageFilter();
}

bool GraphicsContext::hasShadow() const
{
    return !!immutableState()->drawLooper() || !!immutableState()->dropShadowImageFilter();
}

bool GraphicsContext::getTransformedClipBounds(FloatRect* bounds) const
{
    if (contextDisabled())
        return false;
    ASSERT(m_canvas);
    SkIRect skIBounds;
    if (!m_canvas->getClipDeviceBounds(&skIBounds))
        return false;
    SkRect skBounds = SkRect::Make(skIBounds);
    *bounds = FloatRect(skBounds);
    return true;
}

SkMatrix GraphicsContext::getTotalMatrix() const
{
    // FIXME: this is a hack to avoid changing all call sites of getTotalMatrix() to not use this method.
    // The code needs to be cleand up after Slimming Paint is launched.
    if (RuntimeEnabledFeatures::slimmingPaintEnabled())
        return SkMatrix::I();

    if (contextDisabled() || !m_canvas)
        return SkMatrix::I();

    ASSERT(m_canvas);

    if (!isRecording())
        return m_canvas->getTotalMatrix();

    SkMatrix totalMatrix = m_recordingStateStack.last()->matrix();
    totalMatrix.preConcat(m_canvas->getTotalMatrix());

    return totalMatrix;
}

void GraphicsContext::setCompositeOperation(SkXfermode::Mode xferMode)
{
    if (contextDisabled())
        return;
    mutableState()->setCompositeOperation(xferMode);
}

SkXfermode::Mode GraphicsContext::compositeOperation() const
{
    return immutableState()->compositeOperation();
}

CompositeOperator GraphicsContext::compositeOperationDeprecated() const
{
    return compositeOperatorFromSkia(immutableState()->compositeOperation());
}

SkColorFilter* GraphicsContext::colorFilter() const
{
    return immutableState()->colorFilter();
}

void GraphicsContext::setColorFilter(ColorFilter colorFilter)
{
    GraphicsContextState* stateToSet = mutableState();

    // We only support one active color filter at the moment. If (when) this becomes a problem,
    // we should switch to using color filter chains (Skia work in progress).
    ASSERT(!stateToSet->colorFilter());
    stateToSet->setColorFilter(WebCoreColorFilterToSkiaColorFilter(colorFilter));
}

bool GraphicsContext::readPixels(const SkImageInfo& info, void* pixels, size_t rowBytes, int x, int y)
{
    if (contextDisabled())
        return false;

    ASSERT(m_canvas);
    return m_canvas->readPixels(info, pixels, rowBytes, x, y);
}

void GraphicsContext::setMatrix(const SkMatrix& matrix)
{
    if (contextDisabled())
        return;

    ASSERT(m_canvas);

    m_canvas->setMatrix(matrix);
}

void GraphicsContext::concat(const SkMatrix& matrix)
{
    if (contextDisabled())
        return;

    if (matrix.isIdentity())
        return;

    ASSERT(m_canvas);

    m_canvas->concat(matrix);
}

void GraphicsContext::beginTransparencyLayer(float opacity, const FloatRect* bounds)
{
    beginLayer(opacity, immutableState()->compositeOperation(), bounds);
}

void GraphicsContext::beginLayer(float opacity, SkXfermode::Mode xfermode, const FloatRect* bounds, ColorFilter colorFilter, SkImageFilter* imageFilter)
{
    if (contextDisabled())
        return;

    SkPaint layerPaint;
    layerPaint.setAlpha(static_cast<unsigned char>(opacity * 255));
    layerPaint.setXfermodeMode(xfermode);
    layerPaint.setColorFilter(WebCoreColorFilterToSkiaColorFilter(colorFilter).get());
    layerPaint.setImageFilter(imageFilter);

    if (bounds) {
        SkRect skBounds = WebCoreFloatRectToSKRect(*bounds);
        saveLayer(&skBounds, &layerPaint);
    } else {
        saveLayer(0, &layerPaint);
    }

    ++m_layerCount;
}

void GraphicsContext::endLayer()
{
    if (contextDisabled())
        return;

    restoreLayer();

    ASSERT(m_layerCount > 0);
    --m_layerCount;
}

void GraphicsContext::beginRecording(const FloatRect& bounds, uint32_t recordFlags)
{
    if (contextDisabled())
        return;

    m_recordingStateStack.append(
        RecordingState::Create(m_canvas, getTotalMatrix()));
    m_canvas = m_recordingStateStack.last()->recorder().beginRecording(bounds, 0, recordFlags);
}

PassRefPtr<const SkPicture> GraphicsContext::endRecording()
{
    if (contextDisabled())
        return nullptr;

    ASSERT(!m_recordingStateStack.isEmpty());
    RecordingState* recording = m_recordingStateStack.last().get();
    RefPtr<const SkPicture> picture = adoptRef(recording->recorder().endRecordingAsPicture());
    m_canvas = recording->canvas();

    m_recordingStateStack.removeLast();

    ASSERT(picture);
    return picture.release();
}

bool GraphicsContext::isRecording() const
{
    return !m_recordingStateStack.isEmpty();
}

void GraphicsContext::drawPicture(const SkPicture* picture)
{
    // FIXME: SP currently builds empty-bounds pictures in some cases. This is a temp
    // workaround, but the problem should be fixed: empty-bounds pictures are going to be culled
    // on playback anyway.
    bool cullEmptyPictures = !RuntimeEnabledFeatures::slimmingPaintEnabled();
    if (contextDisabled() || !picture || (picture->cullRect().isEmpty() && cullEmptyPictures))
        return;

    ASSERT(m_canvas);
    m_canvas->drawPicture(picture);
}

void GraphicsContext::compositePicture(SkPicture* picture, const FloatRect& dest, const FloatRect& src, SkXfermode::Mode op)
{
    if (contextDisabled() || !picture)
        return;
    ASSERT(m_canvas);

    SkPaint picturePaint;
    picturePaint.setXfermodeMode(op);
    m_canvas->save();
    SkRect sourceBounds = WebCoreFloatRectToSKRect(src);
    SkRect skBounds = WebCoreFloatRectToSKRect(dest);
    SkMatrix pictureTransform;
    pictureTransform.setRectToRect(sourceBounds, skBounds, SkMatrix::kFill_ScaleToFit);
    m_canvas->concat(pictureTransform);
    RefPtr<SkPictureImageFilter> pictureFilter = adoptRef(SkPictureImageFilter::CreateForLocalSpace(picture, sourceBounds, static_cast<SkPaint::FilterLevel>(imageInterpolationQuality())));
    picturePaint.setImageFilter(pictureFilter.get());
    m_canvas->saveLayer(&sourceBounds, &picturePaint);
    m_canvas->restore();
    m_canvas->restore();
}

void GraphicsContext::fillPolygon(size_t numPoints, const FloatPoint* points, const Color& color,
    bool shouldAntialias)
{
    if (contextDisabled())
        return;

    ASSERT(numPoints > 2);

    SkPath path;
    setPathFromPoints(&path, numPoints, points);

    SkPaint paint(immutableState()->fillPaint());
    paint.setAntiAlias(shouldAntialias);
    paint.setColor(color.rgb());

    drawPath(path, paint);
}

float GraphicsContext::prepareFocusRingPaint(SkPaint& paint, const Color& color, int width) const
{
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(color.rgb());
    paint.setStrokeWidth(focusRingWidth(width));

#if OS(MACOSX)
    paint.setAlpha(64);
    return (width - 1) * 0.5f;
#else
    return 1;
#endif
}

void GraphicsContext::drawFocusRingPath(const SkPath& path, const Color& color, int width)
{
    SkPaint paint;
    float cornerRadius = prepareFocusRingPaint(paint, color, width);

    paint.setPathEffect(SkCornerPathEffect::Create(SkFloatToScalar(cornerRadius)))->unref();

    // Outer path
    drawPath(path, paint);

#if OS(MACOSX)
    // Inner path
    paint.setAlpha(128);
    paint.setStrokeWidth(paint.getStrokeWidth() * 0.5f);
    drawPath(path, paint);
#endif
}

void GraphicsContext::drawFocusRingRect(const SkRect& rect, const Color& color, int width)
{
    SkPaint paint;
    float cornerRadius = prepareFocusRingPaint(paint, color, width);

    SkRRect rrect;
    rrect.setRectXY(rect, SkFloatToScalar(cornerRadius), SkFloatToScalar(cornerRadius));

    // Outer rect
    drawRRect(rrect, paint);

#if OS(MACOSX)
    // Inner rect
    paint.setAlpha(128);
    paint.setStrokeWidth(paint.getStrokeWidth() * 0.5f);
    drawRRect(rrect, paint);
#endif
}

void GraphicsContext::drawFocusRing(const Path& focusRingPath, int width, int offset, const Color& color)
{
    // FIXME: Implement support for offset.
    if (contextDisabled())
        return;

    drawFocusRingPath(focusRingPath.skPath(), color, width);
}

void GraphicsContext::drawFocusRing(const Vector<IntRect>& rects, int width, int offset, const Color& color)
{
    if (contextDisabled())
        return;

    unsigned rectCount = rects.size();
    if (!rectCount)
        return;

    SkRegion focusRingRegion;
    const int outset = focusRingOutset(offset);
    for (unsigned i = 0; i < rectCount; i++) {
        SkIRect r = rects[i];
        r.inset(-outset, -outset);
        focusRingRegion.op(r, SkRegion::kUnion_Op);
    }

    if (focusRingRegion.isRect()) {
        drawFocusRingRect(SkRect::MakeFromIRect(focusRingRegion.getBounds()), color, width);
    } else {
        SkPath path;
        if (focusRingRegion.getBoundaryPath(&path))
            drawFocusRingPath(path, color, width);
    }
}

static inline FloatRect areaCastingShadowInHole(const FloatRect& holeRect, int shadowBlur, int shadowSpread, const IntSize& shadowOffset)
{
    IntRect bounds(holeRect);

    bounds.inflate(shadowBlur);

    if (shadowSpread < 0)
        bounds.inflate(-shadowSpread);

    IntRect offsetBounds = bounds;
    offsetBounds.move(-shadowOffset);
    return unionRect(bounds, offsetBounds);
}

void GraphicsContext::drawInnerShadow(const FloatRoundedRect& rect, const Color& shadowColor, const IntSize shadowOffset, int shadowBlur, int shadowSpread, Edges clippedEdges)
{
    if (contextDisabled())
        return;

    FloatRect holeRect(rect.rect());
    holeRect.inflate(-shadowSpread);

    if (holeRect.isEmpty()) {
        if (rect.isRounded())
            fillRoundedRect(rect, shadowColor);
        else
            fillRect(rect.rect(), shadowColor);
        return;
    }

    if (clippedEdges & LeftEdge) {
        holeRect.move(-std::max(shadowOffset.width(), 0) - shadowBlur, 0);
        holeRect.setWidth(holeRect.width() + std::max(shadowOffset.width(), 0) + shadowBlur);
    }
    if (clippedEdges & TopEdge) {
        holeRect.move(0, -std::max(shadowOffset.height(), 0) - shadowBlur);
        holeRect.setHeight(holeRect.height() + std::max(shadowOffset.height(), 0) + shadowBlur);
    }
    if (clippedEdges & RightEdge)
        holeRect.setWidth(holeRect.width() - std::min(shadowOffset.width(), 0) + shadowBlur);
    if (clippedEdges & BottomEdge)
        holeRect.setHeight(holeRect.height() - std::min(shadowOffset.height(), 0) + shadowBlur);

    Color fillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), 255);

    FloatRect outerRect = areaCastingShadowInHole(rect.rect(), shadowBlur, shadowSpread, shadowOffset);
    FloatRoundedRect roundedHole(holeRect, rect.radii());

    save();
    if (rect.isRounded()) {
        Path path;
        path.addRoundedRect(rect);
        clipPath(path);
        if (shadowSpread < 0)
            roundedHole.expandRadii(-shadowSpread);
        else
            roundedHole.shrinkRadii(shadowSpread);
    } else {
        clip(rect.rect());
    }

    OwnPtr<DrawLooperBuilder> drawLooperBuilder = DrawLooperBuilder::create();
    drawLooperBuilder->addShadow(shadowOffset, shadowBlur, shadowColor,
        DrawLooperBuilder::ShadowRespectsTransforms, DrawLooperBuilder::ShadowIgnoresAlpha);
    setDrawLooper(drawLooperBuilder.release());
    fillRectWithRoundedHole(outerRect, roundedHole, fillColor);
    restore();
    clearDrawLooper();
}

void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    StrokeStyle penStyle = strokeStyle();
    if (penStyle == NoStroke)
        return;

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;
    bool isVerticalLine = (p1.x() == p2.x());
    int width = roundf(strokeThickness());

    // We know these are vertical or horizontal lines, so the length will just
    // be the sum of the displacement component vectors give or take 1 -
    // probably worth the speed up of no square root, which also won't be exact.
    FloatSize disp = p2 - p1;
    int length = SkScalarRoundToInt(disp.width() + disp.height());
    SkPaint paint(immutableState()->strokePaint(length));

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
    SkPoint pts[2] = { p1.data(), p2.data() };

    m_canvas->drawPoints(SkCanvas::kLines_PointMode, 2, pts, paint);
}

void GraphicsContext::drawLineForDocumentMarker(const FloatPoint& pt, float width, DocumentMarkerLineStyle style)
{
    if (contextDisabled())
        return;

    // Use 2x resources for a device scale factor of 1.5 or above.
    int deviceScaleFactor = m_deviceScaleFactor > 1.5f ? 2 : 1;

    // Create the pattern we'll use to draw the underline.
    int index = style == DocumentMarkerGrammarLineStyle ? 1 : 0;
    static SkBitmap* misspellBitmap1x[2] = { 0, 0 };
    static SkBitmap* misspellBitmap2x[2] = { 0, 0 };
    SkBitmap** misspellBitmap = deviceScaleFactor == 2 ? misspellBitmap2x : misspellBitmap1x;
    if (!misspellBitmap[index]) {
#if OS(MACOSX)
        // Match the artwork used by the Mac.
        const int rowPixels = 4 * deviceScaleFactor;
        const int colPixels = 3 * deviceScaleFactor;
        SkBitmap bitmap;
        if (!bitmap.tryAllocN32Pixels(rowPixels, colPixels))
            return;

        bitmap.eraseARGB(0, 0, 0, 0);
        const uint32_t transparentColor = 0x00000000;

        if (deviceScaleFactor == 1) {
            const uint32_t colors[2][6] = {
                { 0x2a2a0600, 0x57571000,  0xa8a81b00, 0xbfbf1f00,  0x70701200, 0xe0e02400 },
                { 0x2a0f0f0f, 0x571e1e1e,  0xa83d3d3d, 0xbf454545,  0x70282828, 0xe0515151 }
            };

            // Pattern: a b a   a b a
            //          c d c   c d c
            //          e f e   e f e
            for (int x = 0; x < colPixels; ++x) {
                uint32_t* row = bitmap.getAddr32(0, x);
                row[0] = colors[index][x * 2];
                row[1] = colors[index][x * 2 + 1];
                row[2] = colors[index][x * 2];
                row[3] = transparentColor;
            }
        } else if (deviceScaleFactor == 2) {
            const uint32_t colors[2][18] = {
                { 0x0a090101, 0x33320806, 0x55540f0a,  0x37360906, 0x6e6c120c, 0x6e6c120c,  0x7674140d, 0x8d8b1810, 0x8d8b1810,
                  0x96941a11, 0xb3b01f15, 0xb3b01f15,  0x6d6b130c, 0xd9d62619, 0xd9d62619,  0x19180402, 0x7c7a150e, 0xcecb2418 },
                { 0x0a020202, 0x33141414, 0x55232323,  0x37161616, 0x6e2e2e2e, 0x6e2e2e2e,  0x76313131, 0x8d3a3a3a, 0x8d3a3a3a,
                  0x963e3e3e, 0xb34b4b4b, 0xb34b4b4b,  0x6d2d2d2d, 0xd95b5b5b, 0xd95b5b5b,  0x19090909, 0x7c343434, 0xce575757 }
            };

            // Pattern: a b c c b a
            //          d e f f e d
            //          g h j j h g
            //          k l m m l k
            //          n o p p o n
            //          q r s s r q
            for (int x = 0; x < colPixels; ++x) {
                uint32_t* row = bitmap.getAddr32(0, x);
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

        misspellBitmap[index] = new SkBitmap(bitmap);
#else
        // We use a 2-pixel-high misspelling indicator because that seems to be
        // what WebKit is designed for, and how much room there is in a typical
        // page for it.
        const int rowPixels = 32 * deviceScaleFactor; // Must be multiple of 4 for pattern below.
        const int colPixels = 2 * deviceScaleFactor;
        SkBitmap bitmap;
        if (!bitmap.tryAllocN32Pixels(rowPixels, colPixels))
            return;

        bitmap.eraseARGB(0, 0, 0, 0);
        if (deviceScaleFactor == 1)
            draw1xMarker(&bitmap, index);
        else if (deviceScaleFactor == 2)
            draw2xMarker(&bitmap, index);
        else
            ASSERT_NOT_REACHED();

        misspellBitmap[index] = new SkBitmap(bitmap);
#endif
    }

#if OS(MACOSX)
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

    SkMatrix localMatrix;
    localMatrix.setTranslate(originX, originY);
    RefPtr<SkShader> shader = adoptRef(SkShader::CreateBitmapShader(
        *misspellBitmap[index], SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode, &localMatrix));

    SkPaint paint;
    paint.setShader(shader.get());

    SkRect rect;
    rect.set(originX, originY, originX + WebCoreFloatToSkScalar(width) * deviceScaleFactor, originY + SkIntToScalar(misspellBitmap[index]->height()));

    if (deviceScaleFactor == 2) {
        save();
        scale(0.5, 0.5);
    }
    drawRect(rect, paint);
    if (deviceScaleFactor == 2)
        restore();
}

void GraphicsContext::drawLineForText(const FloatPoint& pt, float width, bool printing)
{
    if (contextDisabled())
        return;

    if (width <= 0)
        return;

    SkPaint paint;
    switch (strokeStyle()) {
    case NoStroke:
    case SolidStroke:
    case DoubleStroke:
    case WavyStroke: {
        int thickness = SkMax32(static_cast<int>(strokeThickness()), 1);
        SkRect r;
        r.fLeft = WebCoreFloatToSkScalar(pt.x());
        // Avoid anti-aliasing lines. Currently, these are always horizontal.
        // Round to nearest pixel to match text and other content.
        r.fTop = WebCoreFloatToSkScalar(floorf(pt.y() + 0.5f));
        r.fRight = r.fLeft + WebCoreFloatToSkScalar(width);
        r.fBottom = r.fTop + SkIntToScalar(thickness);
        paint = immutableState()->fillPaint();
        // Text lines are drawn using the stroke color.
        paint.setColor(effectiveStrokeColor());
        drawRect(r, paint);
        return;
    }
    case DottedStroke:
    case DashedStroke: {
        int y = floorf(pt.y() + std::max<float>(strokeThickness() / 2.0f, 0.5f));
        drawLine(IntPoint(pt.x(), y), IntPoint(pt.x() + width, y));
        return;
    }
    }

    ASSERT_NOT_REACHED();
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (contextDisabled())
        return;

    ASSERT(!rect.isEmpty());
    if (rect.isEmpty())
        return;

    SkRect skRect = rect;
    int fillcolorNotTransparent = immutableState()->fillColor().rgb() & 0xFF000000;
    if (fillcolorNotTransparent)
        drawRect(skRect, immutableState()->fillPaint());

    if (immutableState()->strokeData().style() != NoStroke
        && immutableState()->strokeColor().alpha()) {
        // Stroke a width: 1 inset border
        SkPaint paint(immutableState()->fillPaint());
        paint.setColor(effectiveStrokeColor());
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(1);

        skRect.inset(0.5f, 0.5f);
        drawRect(skRect, paint);
    }
}

void GraphicsContext::drawText(const Font& font, const TextRunPaintInfo& runInfo, const FloatPoint& point)
{
    if (contextDisabled())
        return;

    font.drawText(this, runInfo, point);
}

void GraphicsContext::drawEmphasisMarks(const Font& font, const TextRunPaintInfo& runInfo, const AtomicString& mark, const FloatPoint& point)
{
    if (contextDisabled())
        return;

    font.drawEmphasisMarks(this, runInfo, mark, point);
}

void GraphicsContext::drawBidiText(const Font& font, const TextRunPaintInfo& runInfo, const FloatPoint& point, Font::CustomFontNotReadyAction customFontNotReadyAction)
{
    if (contextDisabled())
        return;

    font.drawBidiText(this, runInfo, point, customFontNotReadyAction);
}

void GraphicsContext::drawHighlightForText(const Font& font, const TextRun& run, const FloatPoint& point, int h, const Color& backgroundColor, int from, int to)
{
    if (contextDisabled())
        return;

    fillRect(font.selectionRectForText(run, point, h, from, to), backgroundColor);
}

void GraphicsContext::drawImage(Image* image, const IntPoint& p, SkXfermode::Mode op, RespectImageOrientationEnum shouldRespectImageOrientation)
{
    if (!image)
        return;
    drawImage(image, FloatRect(IntRect(p, image->size())), FloatRect(FloatPoint(), FloatSize(image->size())), op, shouldRespectImageOrientation);
}

void GraphicsContext::drawImage(Image* image, const IntRect& r, SkXfermode::Mode op, RespectImageOrientationEnum shouldRespectImageOrientation)
{
    if (!image)
        return;
    drawImage(image, FloatRect(r), FloatRect(FloatPoint(), FloatSize(image->size())), op, shouldRespectImageOrientation);
}

void GraphicsContext::drawImage(Image* image, const FloatRect& dest)
{
    if (!image)
        return;
    drawImage(image, dest, FloatRect(IntRect(IntPoint(), image->size())));
}

void GraphicsContext::drawImage(Image* image, const FloatRect& dest, const FloatRect& src, SkXfermode::Mode op, RespectImageOrientationEnum shouldRespectImageOrientation)
{
    if (contextDisabled() || !image)
        return;
    image->draw(this, dest, src, op, shouldRespectImageOrientation);
}

void GraphicsContext::drawTiledImage(Image* image, const IntRect& destRect, const IntPoint& srcPoint, const IntSize& tileSize, SkXfermode::Mode op, const IntSize& repeatSpacing)
{
    if (contextDisabled() || !image)
        return;
    image->drawTiled(this, destRect, srcPoint, tileSize, op, repeatSpacing);
}

void GraphicsContext::drawTiledImage(Image* image, const IntRect& dest, const IntRect& srcRect,
    const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, SkXfermode::Mode op)
{
    if (contextDisabled() || !image)
        return;

    if (hRule == Image::StretchTile && vRule == Image::StretchTile) {
        // Just do a scale.
        drawImage(image, dest, srcRect, op);
        return;
    }

    image->drawTiled(this, dest, srcRect, tileScaleFactor, hRule, vRule, op);
}

void GraphicsContext::drawImageBuffer(ImageBuffer* image, const FloatRect& dest,
    const FloatRect* src, SkXfermode::Mode op)
{
    if (contextDisabled() || !image)
        return;

    image->draw(this, dest, src, op);
}

void GraphicsContext::writePixels(const SkImageInfo& info, const void* pixels, size_t rowBytes, int x, int y)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->writePixels(info, pixels, rowBytes, x, y);
}

void GraphicsContext::drawBitmapRect(const SkBitmap& bitmap, const SkRect* src,
    const SkRect& dst, const SkPaint* paint)
{
    // Textures are bound to the blink main-thread GrContext, which can not be
    // used on the compositor raster thread.
    // FIXME: Mailbox support would make this possible in the GPU-raster case.
    ASSERT(!isRecording() || !bitmap.getTexture());
    if (contextDisabled())
        return;

    SkCanvas::DrawBitmapRectFlags flags =
        immutableState()->shouldClampToSourceRect() ? SkCanvas::kNone_DrawBitmapRectFlag : SkCanvas::kBleed_DrawBitmapRectFlag;

    ASSERT(m_canvas);
    m_canvas->drawBitmapRectToRect(bitmap, src, dst, paint, flags);
}

void GraphicsContext::drawImage(const SkImage* image, SkScalar left, SkScalar top, const SkPaint* paint)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->drawImage(image, left, top, paint);
}

void GraphicsContext::drawImageRect(const SkImage* image, const SkRect* src, const SkRect& dst, const SkPaint* paint)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->drawImageRect(image, src, dst, paint);
}

void GraphicsContext::drawOval(const SkRect& oval, const SkPaint& paint)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->drawOval(oval, paint);
}

void GraphicsContext::drawPath(const SkPath& path, const SkPaint& paint)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->drawPath(path, paint);
}

void GraphicsContext::drawRect(const SkRect& rect, const SkPaint& paint)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->drawRect(rect, paint);
}

void GraphicsContext::drawRRect(const SkRRect& rrect, const SkPaint& paint)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->drawRRect(rrect, paint);
}

void GraphicsContext::drawPosText(const void* text, size_t byteLength,
    const SkPoint pos[], const SkRect& textRect, const SkPaint& paint)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->drawPosText(text, byteLength, pos, paint);
    didDrawTextInRect(textRect);
}

void GraphicsContext::drawPosTextH(const void* text, size_t byteLength,
    const SkScalar xpos[], SkScalar constY, const SkRect& textRect, const SkPaint& paint)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->drawPosTextH(text, byteLength, xpos, constY, paint);
    didDrawTextInRect(textRect);
}

void GraphicsContext::drawTextBlob(const SkTextBlob* blob, const SkPoint& origin, const SkPaint& paint)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->drawTextBlob(blob, origin.x(), origin.y(), paint);

    SkRect bounds = blob->bounds();
    bounds.offset(origin);
    didDrawTextInRect(bounds);
}

void GraphicsContext::fillPath(const Path& pathToFill)
{
    if (contextDisabled() || pathToFill.isEmpty())
        return;

    // Use const_cast and temporarily modify the fill type instead of copying the path.
    SkPath& path = const_cast<SkPath&>(pathToFill.skPath());
    SkPath::FillType previousFillType = path.getFillType();

    SkPath::FillType temporaryFillType = WebCoreWindRuleToSkFillType(immutableState()->fillRule());
    path.setFillType(temporaryFillType);

    drawPath(path, immutableState()->fillPaint());

    path.setFillType(previousFillType);
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (contextDisabled())
        return;

    SkRect r = rect;

    drawRect(r, immutableState()->fillPaint());
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    if (contextDisabled())
        return;

    SkRect r = rect;
    SkPaint paint = immutableState()->fillPaint();
    paint.setColor(color.rgb());
    drawRect(r, paint);
}

void GraphicsContext::fillBetweenRoundedRects(const FloatRect& outer, const FloatSize& outerTopLeft, const FloatSize& outerTopRight, const FloatSize& outerBottomLeft, const FloatSize& outerBottomRight,
    const FloatRect& inner, const FloatSize& innerTopLeft, const FloatSize& innerTopRight, const FloatSize& innerBottomLeft, const FloatSize& innerBottomRight, const Color& color)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    SkVector outerRadii[4];
    SkVector innerRadii[4];
    setRadii(outerRadii, outerTopLeft, outerTopRight, outerBottomRight, outerBottomLeft);
    setRadii(innerRadii, innerTopLeft, innerTopRight, innerBottomRight, innerBottomLeft);

    SkRRect rrOuter;
    SkRRect rrInner;
    rrOuter.setRectRadii(outer, outerRadii);
    rrInner.setRectRadii(inner, innerRadii);

    SkPaint paint(immutableState()->fillPaint());
    paint.setColor(color.rgb());

    m_canvas->drawDRRect(rrOuter, rrInner, paint);
}

void GraphicsContext::fillBetweenRoundedRects(const FloatRoundedRect& outer, const FloatRoundedRect& inner, const Color& color)
{
    fillBetweenRoundedRects(outer.rect(), outer.radii().topLeft(), outer.radii().topRight(), outer.radii().bottomLeft(), outer.radii().bottomRight(),
        inner.rect(), inner.radii().topLeft(), inner.radii().topRight(), inner.radii().bottomLeft(), inner.radii().bottomRight(), color);
}

void GraphicsContext::fillRoundedRect(const FloatRect& rect, const FloatSize& topLeft, const FloatSize& topRight,
    const FloatSize& bottomLeft, const FloatSize& bottomRight, const Color& color)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    if (topLeft.width() + topRight.width() > rect.width()
            || bottomLeft.width() + bottomRight.width() > rect.width()
            || topLeft.height() + bottomLeft.height() > rect.height()
            || topRight.height() + bottomRight.height() > rect.height()) {
        // Not all the radii fit, return a rect. This matches the behavior of
        // Path::createRoundedRectangle. Without this we attempt to draw a round
        // shadow for a square box.
        // FIXME: this fallback code is wrong, and also duplicates related code in FloatRoundedRect::constrainRadii, Path and SKRRect.
        fillRect(rect, color);
        return;
    }

    SkVector radii[4];
    setRadii(radii, topLeft, topRight, bottomRight, bottomLeft);

    SkRRect rr;
    rr.setRectRadii(rect, radii);

    SkPaint paint(immutableState()->fillPaint());
    paint.setColor(color.rgb());

    m_canvas->drawRRect(rr, paint);
}

void GraphicsContext::fillEllipse(const FloatRect& ellipse)
{
    if (contextDisabled())
        return;

    SkRect rect = ellipse;
    drawOval(rect, immutableState()->fillPaint());
}

void GraphicsContext::strokePath(const Path& pathToStroke)
{
    if (contextDisabled() || pathToStroke.isEmpty())
        return;

    const SkPath& path = pathToStroke.skPath();
    drawPath(path, immutableState()->strokePaint());
}

void GraphicsContext::strokeRect(const FloatRect& rect)
{
    strokeRect(rect, strokeThickness());
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (contextDisabled())
        return;

    SkPaint paint(immutableState()->strokePaint());
    paint.setStrokeWidth(WebCoreFloatToSkScalar(lineWidth));
    // Reset the dash effect to account for the width
    immutableState()->strokeData().setupPaintDashPathEffect(&paint, 0);
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
    if (contextDisabled())
        return;

    drawOval(ellipse, immutableState()->strokePaint());
}

void GraphicsContext::clipRoundedRect(const FloatRoundedRect& rect, SkRegion::Op regionOp)
{
    if (contextDisabled())
        return;

    if (!rect.isRounded()) {
        clipRect(rect.rect(), NotAntiAliased, regionOp);
        return;
    }

    SkVector radii[4];
    FloatRoundedRect::Radii wkRadii = rect.radii();
    setRadii(radii, wkRadii.topLeft(), wkRadii.topRight(), wkRadii.bottomRight(), wkRadii.bottomLeft());

    SkRRect r;
    r.setRectRadii(rect.rect(), radii);

    clipRRect(r, AntiAliased, regionOp);
}

void GraphicsContext::clipOut(const Path& pathToClip)
{
    if (contextDisabled())
        return;

    // Use const_cast and temporarily toggle the inverse fill type instead of copying the path.
    SkPath& path = const_cast<SkPath&>(pathToClip.skPath());
    path.toggleInverseFillType();
    clipPath(path, AntiAliased);
    path.toggleInverseFillType();
}

void GraphicsContext::clipPath(const Path& pathToClip, WindRule clipRule, AntiAliasingMode antiAliasingMode)
{
    if (contextDisabled())
        return;

    // Use const_cast and temporarily modify the fill type instead of copying the path.
    SkPath& path = const_cast<SkPath&>(pathToClip.skPath());
    SkPath::FillType previousFillType = path.getFillType();

    SkPath::FillType temporaryFillType = WebCoreWindRuleToSkFillType(clipRule);
    path.setFillType(temporaryFillType);
    clipPath(path, antiAliasingMode);

    path.setFillType(previousFillType);
}

void GraphicsContext::clipPolygon(size_t numPoints, const FloatPoint* points, bool antialiased)
{
    if (contextDisabled())
        return;

    ASSERT(numPoints > 2);

    SkPath path;
    setPathFromPoints(&path, numPoints, points);
    clipPath(path, antialiased ? AntiAliased : NotAntiAliased);
}

void GraphicsContext::clipOutRoundedRect(const FloatRoundedRect& rect)
{
    if (contextDisabled())
        return;

    clipRoundedRect(rect, SkRegion::kDifference_Op);
}

void GraphicsContext::clipRect(const SkRect& rect, AntiAliasingMode aa, SkRegion::Op op)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->clipRect(rect, op, aa == AntiAliased);
}

void GraphicsContext::clipPath(const SkPath& path, AntiAliasingMode aa, SkRegion::Op op)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->clipPath(path, op, aa == AntiAliased);
}

void GraphicsContext::clipRRect(const SkRRect& rect, AntiAliasingMode aa, SkRegion::Op op)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->clipRRect(rect, op, aa == AntiAliased);
}

void GraphicsContext::rotate(float angleInRadians)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    m_canvas->rotate(WebCoreFloatToSkScalar(angleInRadians * (180.0f / 3.14159265f)));
}

void GraphicsContext::translate(float x, float y)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    if (!x && !y)
        return;

    m_canvas->translate(WebCoreFloatToSkScalar(x), WebCoreFloatToSkScalar(y));
}

void GraphicsContext::scale(float x, float y)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    if (x == 1.0f && y == 1.0f)
        return;

    m_canvas->scale(WebCoreFloatToSkScalar(x), WebCoreFloatToSkScalar(y));
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    SkAutoDataUnref url(SkData::NewWithCString(link.string().utf8().data()));
    SkAnnotateRectWithURL(m_canvas, destRect, url.get());
}

void GraphicsContext::setURLFragmentForRect(const String& destName, const IntRect& rect)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    SkAutoDataUnref skDestName(SkData::NewWithCString(destName.utf8().data()));
    SkAnnotateLinkToDestination(m_canvas, rect, skDestName.get());
}

void GraphicsContext::addURLTargetAtPoint(const String& name, const IntPoint& pos)
{
    if (contextDisabled())
        return;
    ASSERT(m_canvas);

    SkAutoDataUnref nameData(SkData::NewWithCString(name.utf8().data()));
    SkAnnotateNamedDestination(m_canvas, SkPoint::Make(pos.x(), pos.y()), nameData);
}

AffineTransform GraphicsContext::getCTM() const
{
    if (contextDisabled())
        return AffineTransform();

    SkMatrix m = getTotalMatrix();
    return AffineTransform(SkScalarToDouble(m.getScaleX()),
                           SkScalarToDouble(m.getSkewY()),
                           SkScalarToDouble(m.getSkewX()),
                           SkScalarToDouble(m.getScaleY()),
                           SkScalarToDouble(m.getTranslateX()),
                           SkScalarToDouble(m.getTranslateY()));
}

void GraphicsContext::concatCTM(const AffineTransform& affine)
{
    concat(affineTransformToSkMatrix(affine));
}

void GraphicsContext::setCTM(const AffineTransform& affine)
{
    setMatrix(affineTransformToSkMatrix(affine));
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, SkXfermode::Mode op)
{
    if (contextDisabled())
        return;

    SkXfermode::Mode previousOperation = compositeOperation();
    setCompositeOperation(op);
    fillRect(rect, color);
    setCompositeOperation(previousOperation);
}

void GraphicsContext::fillRoundedRect(const FloatRoundedRect& rect, const Color& color)
{
    if (contextDisabled())
        return;

    if (rect.isRounded())
        fillRoundedRect(rect.rect(), rect.radii().topLeft(), rect.radii().topRight(), rect.radii().bottomLeft(), rect.radii().bottomRight(), color);
    else
        fillRect(rect.rect(), color);
}

void GraphicsContext::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    if (contextDisabled())
        return;

    Path path;
    path.addRect(rect);

    if (!roundedHoleRect.radii().isZero())
        path.addRoundedRect(roundedHoleRect);
    else
        path.addRect(roundedHoleRect.rect());

    WindRule oldFillRule = fillRule();
    Color oldFillColor = fillColor();

    setFillRule(RULE_EVENODD);
    setFillColor(color);

    fillPath(path);

    setFillRule(oldFillRule);
    setFillColor(oldFillColor);
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (contextDisabled())
        return;

    SkRect r = rect;
    SkPaint paint(immutableState()->fillPaint());
    paint.setXfermodeMode(SkXfermode::kClear_Mode);
    drawRect(r, paint);
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

void GraphicsContext::setPathFromPoints(SkPath* path, size_t numPoints, const FloatPoint* points)
{
    path->incReserve(numPoints);
    path->moveTo(WebCoreFloatToSkScalar(points[0].x()),
                 WebCoreFloatToSkScalar(points[0].y()));
    for (size_t i = 1; i < numPoints; ++i) {
        path->lineTo(WebCoreFloatToSkScalar(points[i].x()),
                     WebCoreFloatToSkScalar(points[i].y()));
    }
}

void GraphicsContext::setRadii(SkVector* radii, FloatSize topLeft, FloatSize topRight, FloatSize bottomRight, FloatSize bottomLeft)
{
    radii[SkRRect::kUpperLeft_Corner].set(SkFloatToScalar(topLeft.width()),
        SkFloatToScalar(topLeft.height()));
    radii[SkRRect::kUpperRight_Corner].set(SkFloatToScalar(topRight.width()),
        SkFloatToScalar(topRight.height()));
    radii[SkRRect::kLowerRight_Corner].set(SkFloatToScalar(bottomRight.width()),
        SkFloatToScalar(bottomRight.height()));
    radii[SkRRect::kLowerLeft_Corner].set(SkFloatToScalar(bottomLeft.width()),
        SkFloatToScalar(bottomLeft.height()));
}

PassRefPtr<SkColorFilter> GraphicsContext::WebCoreColorFilterToSkiaColorFilter(ColorFilter colorFilter)
{
    switch (colorFilter) {
    case ColorFilterLuminanceToAlpha:
        return adoptRef(SkLumaColorFilter::Create());
    case ColorFilterLinearRGBToSRGB:
        return ImageBuffer::createColorSpaceFilter(ColorSpaceLinearRGB, ColorSpaceDeviceRGB);
    case ColorFilterSRGBToLinearRGB:
        return ImageBuffer::createColorSpaceFilter(ColorSpaceDeviceRGB, ColorSpaceLinearRGB);
    case ColorFilterNone:
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return nullptr;
}

#if !OS(MACOSX)
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

SkPMColor GraphicsContext::lineColors(int index)
{
    static const SkPMColor colors[] = {
        SkPreMultiplyARGB(0xFF, 0xFF, 0x00, 0x00), // Opaque red.
        SkPreMultiplyARGB(0xFF, 0xC0, 0xC0, 0xC0) // Opaque gray.
    };

    return colors[index];
}

SkPMColor GraphicsContext::antiColors1(int index)
{
    static const SkPMColor colors[] = {
        SkPreMultiplyARGB(0xB0, 0xFF, 0x00, 0x00), // Semitransparent red.
        SkPreMultiplyARGB(0xB0, 0xC0, 0xC0, 0xC0)  // Semitransparent gray.
    };

    return colors[index];
}

SkPMColor GraphicsContext::antiColors2(int index)
{
    static const SkPMColor colors[] = {
        SkPreMultiplyARGB(0x60, 0xFF, 0x00, 0x00), // More transparent red
        SkPreMultiplyARGB(0x60, 0xC0, 0xC0, 0xC0)  // More transparent gray
    };

    return colors[index];
}
#endif

void GraphicsContext::didDrawTextInRect(const SkRect& textRect)
{
    if (m_trackTextRegion) {
        TRACE_EVENT0("skia", "GraphicsContext::didDrawTextInRect");
        m_textRegion.join(textRect);
    }
}

int GraphicsContext::preparePaintForDrawRectToRect(
    SkPaint* paint,
    const SkRect& srcRect,
    const SkRect& destRect,
    SkXfermode::Mode compositeOp,
    bool isBitmapWithAlpha,
    bool isLazyDecoded,
    bool isDataComplete) const
{
    int initialSaveCount = m_canvas->getSaveCount();

    paint->setColorFilter(this->colorFilter());
    paint->setAlpha(this->getNormalizedAlpha());
    bool usingImageFilter = false;
    if (dropShadowImageFilter() && isBitmapWithAlpha) {
        SkMatrix ctm = getTotalMatrix();
        SkMatrix invCtm;
        if (ctm.invert(&invCtm)) {
            usingImageFilter = true;
            // The image filter is meant to ignore tranforms with respect to
            // the shadow parameters. The matrix tweaks below ensures that the image
            // filter is applied in post-transform space. We use concat() instead of
            // setMatrix() in case this goes into a recording canvas which may need to
            // respect a parent transform at playback time.
            m_canvas->save();
            m_canvas->concat(invCtm);
            SkRect bounds = destRect;
            ctm.mapRect(&bounds);
            SkRect filteredBounds;
            dropShadowImageFilter()->computeFastBounds(bounds, &filteredBounds);
            SkPaint layerPaint;
            layerPaint.setXfermodeMode(compositeOp);
            layerPaint.setImageFilter(dropShadowImageFilter());
            m_canvas->saveLayer(&filteredBounds, &layerPaint);
            m_canvas->concat(ctm);
        }
    }

    if (!usingImageFilter) {
        paint->setXfermodeMode(compositeOp);
        paint->setLooper(this->drawLooper());
    }

    paint->setAntiAlias(shouldDrawAntiAliased(this, destRect));

    InterpolationQuality resampling;
    if (this->isAccelerated()) {
        resampling = InterpolationLow;
    } else if (this->printing()) {
        resampling = InterpolationNone;
    } else if (isLazyDecoded) {
        resampling = InterpolationHigh;
    } else {
        // Take into account scale applied to the canvas when computing sampling mode (e.g. CSS scale or page scale).
        SkRect destRectTarget = destRect;
        SkMatrix totalMatrix = this->getTotalMatrix();
        if (!(totalMatrix.getType() & (SkMatrix::kAffine_Mask | SkMatrix::kPerspective_Mask)))
            totalMatrix.mapRect(&destRectTarget, destRect);

        resampling = computeInterpolationQuality(totalMatrix,
            SkScalarToFloat(srcRect.width()), SkScalarToFloat(srcRect.height()),
            SkScalarToFloat(destRectTarget.width()), SkScalarToFloat(destRectTarget.height()),
            isDataComplete);
    }

    if (resampling == InterpolationNone) {
        // FIXME: This is to not break tests (it results in the filter bitmap flag
        // being set to true). We need to decide if we respect InterpolationNone
        // being returned from computeInterpolationQuality.
        resampling = InterpolationLow;
    }
    resampling = limitInterpolationQuality(this, resampling);
    paint->setFilterLevel(static_cast<SkPaint::FilterLevel>(resampling));

    return initialSaveCount;
}

} // namespace blink
