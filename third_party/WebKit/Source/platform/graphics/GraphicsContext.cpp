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

#include "platform/graphics/GraphicsContext.h"

#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/ColorSpace.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/Path.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/paint/PaintRecorder.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/weborigin/KURL.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkAnnotation.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"
#include "third_party/skia/include/effects/SkPictureImageFilter.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"
#include "wtf/Assertions.h"
#include "wtf/MathExtras.h"
#include <memory>

namespace blink {

GraphicsContext::GraphicsContext(PaintController& paintController,
                                 DisabledMode disableContextOrPainting,
                                 SkMetaData* metaData,
                                 ColorBehavior colorBehavior)
    : m_canvas(nullptr),
      m_paintController(paintController),
      m_paintStateStack(),
      m_paintStateIndex(0),
      m_colorBehavior(colorBehavior),
#if DCHECK_IS_ON()
      m_layerCount(0),
      m_disableDestructionChecks(false),
      m_inDrawingRecorder(false),
#endif
      m_disabledState(disableContextOrPainting),
      m_deviceScaleFactor(1.0f),
      m_printing(false),
      m_hasMetaData(!!metaData) {
  if (metaData)
    m_metaData = *metaData;

  // FIXME: Do some tests to determine how many states are typically used, and
  // allocate several here.
  m_paintStateStack.push_back(GraphicsContextState::create());
  m_paintState = m_paintStateStack.back().get();

  if (contextDisabled()) {
    DEFINE_STATIC_LOCAL(SkCanvas*, nullSkCanvas,
                        (SkMakeNullCanvas().release()));
    DEFINE_STATIC_LOCAL(PaintCanvasPassThrough, nullCanvas, (nullSkCanvas));
    m_canvas = &nullCanvas;
  }
}

GraphicsContext::~GraphicsContext() {
#if DCHECK_IS_ON()
  if (!m_disableDestructionChecks) {
    ASSERT(!m_paintStateIndex);
    ASSERT(!m_paintState->saveCount());
    ASSERT(!m_layerCount);
    ASSERT(!saveCount());
  }
#endif
}

void GraphicsContext::save() {
  if (contextDisabled())
    return;

  m_paintState->incrementSaveCount();

  ASSERT(m_canvas);
  m_canvas->save();
}

void GraphicsContext::restore() {
  if (contextDisabled())
    return;

  if (!m_paintStateIndex && !m_paintState->saveCount()) {
    DLOG(ERROR) << "ERROR void GraphicsContext::restore() stack is empty";
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

#if DCHECK_IS_ON()
unsigned GraphicsContext::saveCount() const {
  // Each m_paintStateStack entry implies an additional save op
  // (on top of its own saveCount), except for the first frame.
  unsigned count = m_paintStateIndex;
  ASSERT(m_paintStateStack.size() > m_paintStateIndex);
  for (unsigned i = 0; i <= m_paintStateIndex; ++i)
    count += m_paintStateStack[i]->saveCount();

  return count;
}
#endif

void GraphicsContext::saveLayer(const SkRect* bounds, const PaintFlags* paint) {
  if (contextDisabled())
    return;

  ASSERT(m_canvas);
  m_canvas->saveLayer(bounds, paint);
}

void GraphicsContext::restoreLayer() {
  if (contextDisabled())
    return;

  ASSERT(m_canvas);

  m_canvas->restore();
}

#if DCHECK_IS_ON()
void GraphicsContext::setInDrawingRecorder(bool val) {
  // Nested drawing recorers are not allowed.
  ASSERT(!val || !m_inDrawingRecorder);
  m_inDrawingRecorder = val;
}
#endif

void GraphicsContext::setShadow(
    const FloatSize& offset,
    float blur,
    const Color& color,
    DrawLooperBuilder::ShadowTransformMode shadowTransformMode,
    DrawLooperBuilder::ShadowAlphaMode shadowAlphaMode,
    ShadowMode shadowMode) {
  if (contextDisabled())
    return;

  DrawLooperBuilder drawLooperBuilder;
  if (!color.alpha()) {
    // When shadow-only but there is no shadow, we use an empty draw looper
    // to disable rendering of the source primitive.  When not shadow-only, we
    // clear the looper.
    setDrawLooper(shadowMode != DrawShadowOnly
                      ? nullptr
                      : drawLooperBuilder.detachDrawLooper());
    return;
  }

  drawLooperBuilder.addShadow(offset, blur, color, shadowTransformMode,
                              shadowAlphaMode);
  if (shadowMode == DrawShadowAndForeground) {
    drawLooperBuilder.addUnmodifiedContent();
  }
  setDrawLooper(drawLooperBuilder.detachDrawLooper());
}

void GraphicsContext::setDrawLooper(sk_sp<SkDrawLooper> drawLooper) {
  if (contextDisabled())
    return;

  mutableState()->setDrawLooper(std::move(drawLooper));
}

SkColorFilter* GraphicsContext::getColorFilter() const {
  return immutableState()->getColorFilter();
}

void GraphicsContext::setColorFilter(ColorFilter colorFilter) {
  GraphicsContextState* stateToSet = mutableState();

  // We only support one active color filter at the moment. If (when) this
  // becomes a problem, we should switch to using color filter chains (Skia work
  // in progress).
  DCHECK(!stateToSet->getColorFilter());
  stateToSet->setColorFilter(WebCoreColorFilterToSkiaColorFilter(colorFilter));
}

void GraphicsContext::concat(const SkMatrix& matrix) {
  if (contextDisabled())
    return;

  ASSERT(m_canvas);

  m_canvas->concat(matrix);
}

void GraphicsContext::beginLayer(float opacity,
                                 SkBlendMode xfermode,
                                 const FloatRect* bounds,
                                 ColorFilter colorFilter,
                                 sk_sp<SkImageFilter> imageFilter) {
  if (contextDisabled())
    return;

  PaintFlags layerPaint;
  layerPaint.setAlpha(static_cast<unsigned char>(opacity * 255));
  layerPaint.setBlendMode(xfermode);
  layerPaint.setColorFilter(WebCoreColorFilterToSkiaColorFilter(colorFilter));
  layerPaint.setImageFilter(std::move(imageFilter));

  if (bounds) {
    SkRect skBounds = *bounds;
    saveLayer(&skBounds, &layerPaint);
  } else {
    saveLayer(nullptr, &layerPaint);
  }

#if DCHECK_IS_ON()
  ++m_layerCount;
#endif
}

void GraphicsContext::endLayer() {
  if (contextDisabled())
    return;

  restoreLayer();

#if DCHECK_IS_ON()
  DCHECK_GT(m_layerCount--, 0);
#endif
}

void GraphicsContext::beginRecording(const FloatRect& bounds) {
  if (contextDisabled())
    return;

  DCHECK(!m_canvas);
  m_canvas = m_pictureRecorder.beginRecording(bounds, nullptr);
  if (m_hasMetaData)
    skia::GetMetaData(*m_canvas) = m_metaData;
}

namespace {

sk_sp<PaintRecord> createEmptyPicture() {
  PaintRecorder recorder;
  recorder.beginRecording(SkRect::MakeEmpty(), nullptr);
  return recorder.finishRecordingAsPicture();
}

}  // anonymous namespace

sk_sp<PaintRecord> GraphicsContext::endRecording() {
  if (contextDisabled()) {
    // Clients expect endRecording() to always return a non-null picture.
    // Cache an empty SKP to minimize overhead when disabled.
    DEFINE_STATIC_LOCAL(sk_sp<PaintRecord>, emptyPicture,
                        (createEmptyPicture()));
    return emptyPicture;
  }

  sk_sp<PaintRecord> picture = m_pictureRecorder.finishRecordingAsPicture();
  m_canvas = nullptr;
  ASSERT(picture);
  return picture;
}

void GraphicsContext::drawPicture(const PaintRecord* picture) {
  if (contextDisabled() || !picture || picture->cullRect().isEmpty())
    return;

  ASSERT(m_canvas);
  m_canvas->drawPicture(picture);
}

void GraphicsContext::compositePicture(sk_sp<PaintRecord> picture,
                                       const FloatRect& dest,
                                       const FloatRect& src,
                                       SkBlendMode op) {
  if (contextDisabled() || !picture)
    return;
  ASSERT(m_canvas);

  PaintFlags picturePaint;
  picturePaint.setBlendMode(op);
  m_canvas->save();
  SkRect sourceBounds = src;
  SkRect skBounds = dest;
  SkMatrix pictureTransform;
  pictureTransform.setRectToRect(sourceBounds, skBounds,
                                 SkMatrix::kFill_ScaleToFit);
  m_canvas->concat(pictureTransform);
  picturePaint.setImageFilter(SkPictureImageFilter::MakeForLocalSpace(
      ToSkPicture(picture), sourceBounds,
      static_cast<SkFilterQuality>(imageInterpolationQuality())));
  m_canvas->saveLayer(&sourceBounds, &picturePaint);
  m_canvas->restore();
  m_canvas->restore();
}

void GraphicsContext::drawFocusRingPath(const SkPath& path,
                                        const Color& color,
                                        float width) {
  drawPlatformFocusRing(path, m_canvas, color.rgb(), width);
}

void GraphicsContext::drawFocusRingRect(const SkRect& rect,
                                        const Color& color,
                                        float width) {
  drawPlatformFocusRing(rect, m_canvas, color.rgb(), width);
}

void GraphicsContext::drawFocusRing(const Path& focusRingPath,
                                    float width,
                                    int offset,
                                    const Color& color) {
  // FIXME: Implement support for offset.
  if (contextDisabled())
    return;

  drawFocusRingPath(focusRingPath.getSkPath(), color, width);
}

void GraphicsContext::drawFocusRing(const Vector<IntRect>& rects,
                                    float width,
                                    int offset,
                                    const Color& color) {
  if (contextDisabled())
    return;

  unsigned rectCount = rects.size();
  if (!rectCount)
    return;

  SkRegion focusRingRegion;
  offset = focusRingOffset(offset);
  for (unsigned i = 0; i < rectCount; i++) {
    SkIRect r = rects[i];
    if (r.isEmpty())
      continue;
    r.inset(-offset, -offset);
    focusRingRegion.op(r, SkRegion::kUnion_Op);
  }

  if (focusRingRegion.isEmpty())
    return;

  if (focusRingRegion.isRect()) {
    drawFocusRingRect(SkRect::Make(focusRingRegion.getBounds()), color, width);
  } else {
    SkPath path;
    if (focusRingRegion.getBoundaryPath(&path))
      drawFocusRingPath(path, color, width);
  }
}

static inline FloatRect areaCastingShadowInHole(const FloatRect& holeRect,
                                                float shadowBlur,
                                                float shadowSpread,
                                                const FloatSize& shadowOffset) {
  FloatRect bounds(holeRect);

  bounds.inflate(shadowBlur);

  if (shadowSpread < 0)
    bounds.inflate(-shadowSpread);

  FloatRect offsetBounds = bounds;
  offsetBounds.move(-shadowOffset);
  return unionRect(bounds, offsetBounds);
}

void GraphicsContext::drawInnerShadow(const FloatRoundedRect& rect,
                                      const Color& shadowColor,
                                      const FloatSize& shadowOffset,
                                      float shadowBlur,
                                      float shadowSpread,
                                      Edges clippedEdges) {
  if (contextDisabled())
    return;

  FloatRect holeRect(rect.rect());
  holeRect.inflate(-shadowSpread);

  if (holeRect.isEmpty()) {
    fillRoundedRect(rect, shadowColor);
    return;
  }

  if (clippedEdges & LeftEdge) {
    holeRect.move(-std::max(shadowOffset.width(), 0.0f) - shadowBlur, 0);
    holeRect.setWidth(holeRect.width() + std::max(shadowOffset.width(), 0.0f) +
                      shadowBlur);
  }
  if (clippedEdges & TopEdge) {
    holeRect.move(0, -std::max(shadowOffset.height(), 0.0f) - shadowBlur);
    holeRect.setHeight(holeRect.height() +
                       std::max(shadowOffset.height(), 0.0f) + shadowBlur);
  }
  if (clippedEdges & RightEdge)
    holeRect.setWidth(holeRect.width() - std::min(shadowOffset.width(), 0.0f) +
                      shadowBlur);
  if (clippedEdges & BottomEdge)
    holeRect.setHeight(holeRect.height() -
                       std::min(shadowOffset.height(), 0.0f) + shadowBlur);

  Color fillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(),
                  255);

  FloatRect outerRect = areaCastingShadowInHole(rect.rect(), shadowBlur,
                                                shadowSpread, shadowOffset);
  FloatRoundedRect roundedHole(holeRect, rect.getRadii());

  GraphicsContextStateSaver stateSaver(*this);
  if (rect.isRounded()) {
    clipRoundedRect(rect);
    if (shadowSpread < 0)
      roundedHole.expandRadii(-shadowSpread);
    else
      roundedHole.shrinkRadii(shadowSpread);
  } else {
    clip(rect.rect());
  }

  DrawLooperBuilder drawLooperBuilder;
  drawLooperBuilder.addShadow(FloatSize(shadowOffset), shadowBlur, shadowColor,
                              DrawLooperBuilder::ShadowRespectsTransforms,
                              DrawLooperBuilder::ShadowIgnoresAlpha);
  setDrawLooper(drawLooperBuilder.detachDrawLooper());
  fillRectWithRoundedHole(outerRect, roundedHole, fillColor);
}

void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2) {
  if (contextDisabled())
    return;
  ASSERT(m_canvas);

  StrokeStyle penStyle = getStrokeStyle();
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
  PaintFlags paint(immutableState()->strokePaint(length));

  if (getStrokeStyle() == DottedStroke || getStrokeStyle() == DashedStroke) {
    // Do a rect fill of our endpoints.  This ensures we always have the
    // appearance of being a border.  We then draw the actual dotted/dashed
    // line.
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
    PaintFlags fillPaint;
    fillPaint.setColor(paint.getColor());
    drawRect(r1, fillPaint);
    drawRect(r2, fillPaint);
  }

  adjustLineToPixelBoundaries(p1, p2, width, penStyle);
  m_canvas->drawLine(p1.x(), p1.y(), p2.x(), p2.y(), paint);
}

namespace {

#if !OS(MACOSX)

sk_sp<SkPicture> recordMarker(GraphicsContext::DocumentMarkerLineStyle style) {
  SkColor color = (style == GraphicsContext::DocumentMarkerGrammarLineStyle)
      ? SkColorSetRGB(0xC0, 0xC0, 0xC0)
      : SK_ColorRED;

  // Record the path equivalent to this legacy pattern:
  //   X o   o X o   o X
  //     o X o   o X o

  static const float kW = 4;
  static const float kH = 2;

  // Adjust the phase such that f' == 0 is "pixel"-centered
  // (for optimal rasterization at native rez).
  SkPath path;
  path.moveTo(kW * -3 / 8, kH * 3 / 4);
  path.cubicTo(kW * -1 / 8, kH * 3 / 4,
               kW * -1 / 8, kH * 1 / 4,
               kW *  1 / 8, kH * 1 / 4);
  path.cubicTo(kW * 3 / 8, kH * 1 / 4,
               kW * 3 / 8, kH * 3 / 4,
               kW * 5 / 8, kH * 3 / 4);
  path.cubicTo(kW * 7 / 8, kH * 3 / 4,
               kW * 7 / 8, kH * 1 / 4,
               kW * 9 / 8, kH * 1 / 4);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(kH * 1 / 2);

  SkPictureRecorder recorder;
  recorder.beginRecording(kW, kH);
  recorder.getRecordingCanvas()->drawPath(path, paint);

  return recorder.finishRecordingAsPicture();
}

#else  // OS(MACOSX)

sk_sp<SkPicture> recordMarker(GraphicsContext::DocumentMarkerLineStyle style) {
  SkColor color = (style == GraphicsContext::DocumentMarkerGrammarLineStyle)
      ? SkColorSetRGB(0x6B, 0x6B, 0x6B)
      : SkColorSetRGB(0xFB, 0x2D, 0x1D);

  // Match the artwork used by the Mac.
  static const float kW = 4;
  static const float kH = 3;
  static const float kR = 1.5f;

  // top->bottom translucent gradient.
  const SkColor colors[2] = {
      SkColorSetARGB(0x48,
                     SkColorGetR(color),
                     SkColorGetG(color),
                     SkColorGetB(color)),
      color
  };
  const SkPoint pts[2] = {
      SkPoint::Make(0, 0),
      SkPoint::Make(0, 2 * kR)
  };

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);
  paint.setShader(SkGradientShader::MakeLinear(pts,
                                               colors,
                                               nullptr,
                                               ARRAY_SIZE(colors),
                                               SkShader::kClamp_TileMode));
  SkPictureRecorder recorder;
  recorder.beginRecording(kW, kH);
  recorder.getRecordingCanvas()->drawCircle(kR, kR, kR, paint);

  return recorder.finishRecordingAsPicture();
}

#endif  // OS(MACOSX)

}  // anonymous ns

void GraphicsContext::drawLineForDocumentMarker(const FloatPoint& pt,
                                                float width,
                                                DocumentMarkerLineStyle style,
                                                float zoom) {
  if (contextDisabled())
    return;

  DEFINE_STATIC_LOCAL(SkPicture*, spellingMarker,
                      (recordMarker(DocumentMarkerSpellingLineStyle).release()));
  DEFINE_STATIC_LOCAL(SkPicture*, grammarMarker,
                      (recordMarker(DocumentMarkerGrammarLineStyle).release()));
  const auto& marker = style == DocumentMarkerSpellingLineStyle
      ? spellingMarker
      : grammarMarker;

  // Position already includes zoom and device scale factor.
  SkScalar originX = WebCoreFloatToSkScalar(pt.x());
  SkScalar originY = WebCoreFloatToSkScalar(pt.y());

#if OS(MACOSX)
  // Make sure to draw only complete dots, and finish inside the marked text.
  width -= fmodf(width, marker->cullRect().width() * zoom);
#else
  // Offset it vertically by 1 so that there's some space under the text.
  originY += 1;
#endif

  const auto rect = SkRect::MakeWH(width, marker->cullRect().height() * zoom);
  const auto localMatrix = SkMatrix::MakeScale(zoom, zoom);

  PaintFlags paint;
  paint.setAntiAlias(true);
  paint.setShader(WrapSkShader(
      SkShader::MakePictureShader(sk_ref_sp(marker),
                                  SkShader::kRepeat_TileMode,
                                  SkShader::kClamp_TileMode,
                                  &localMatrix,
                                  nullptr)));

  // Apply the origin translation as a global transform.  This ensures that the
  // shader local matrix depends solely on zoom => Skia can reuse the same
  // cached tile for all markers at a given zoom level.
  SkAutoCanvasRestore acr(m_canvas, true);
  m_canvas->translate(originX, originY);
  m_canvas->drawRect(rect, paint);
}

void GraphicsContext::drawLineForText(const FloatPoint& pt, float width) {
  if (contextDisabled())
    return;

  if (width <= 0)
    return;

  PaintFlags paint;
  switch (getStrokeStyle()) {
    case NoStroke:
    case SolidStroke:
    case DoubleStroke: {
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
      paint.setColor(strokeColor().rgb());
      drawRect(r, paint);
      return;
    }
    case DottedStroke:
    case DashedStroke: {
      int y = floorf(pt.y() + std::max<float>(strokeThickness() / 2.0f, 0.5f));
      drawLine(IntPoint(pt.x(), y), IntPoint(pt.x() + width, y));
      return;
    }
    case WavyStroke:
    default:
      break;
  }

  ASSERT_NOT_REACHED();
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect) {
  if (contextDisabled())
    return;

  ASSERT(!rect.isEmpty());
  if (rect.isEmpty())
    return;

  SkRect skRect = rect;
  if (immutableState()->fillColor().alpha())
    drawRect(skRect, immutableState()->fillPaint());

  if (immutableState()->getStrokeData().style() != NoStroke &&
      immutableState()->strokeColor().alpha()) {
    // Stroke a width: 1 inset border
    PaintFlags paint(immutableState()->fillPaint());
    paint.setColor(strokeColor().rgb());
    paint.setStyle(PaintFlags::kStroke_Style);
    paint.setStrokeWidth(1);

    skRect.inset(0.5f, 0.5f);
    drawRect(skRect, paint);
  }
}

void GraphicsContext::drawText(const Font& font,
                               const TextRunPaintInfo& runInfo,
                               const FloatPoint& point,
                               const PaintFlags& paint) {
  if (contextDisabled())
    return;

  if (font.drawText(m_canvas, runInfo, point, m_deviceScaleFactor, paint))
    m_paintController.setTextPainted();
}

template <typename DrawTextFunc>
void GraphicsContext::drawTextPasses(const DrawTextFunc& drawText) {
  TextDrawingModeFlags modeFlags = textDrawingMode();

  if (modeFlags & TextModeFill) {
    drawText(immutableState()->fillPaint());
  }

  if ((modeFlags & TextModeStroke) && getStrokeStyle() != NoStroke &&
      strokeThickness() > 0) {
    PaintFlags paintForStroking(immutableState()->strokePaint());
    if (modeFlags & TextModeFill) {
      paintForStroking.setLooper(
          0);  // shadow was already applied during fill pass
    }
    drawText(paintForStroking);
  }
}

void GraphicsContext::drawText(const Font& font,
                               const TextRunPaintInfo& runInfo,
                               const FloatPoint& point) {
  if (contextDisabled())
    return;

  drawTextPasses([&font, &runInfo, &point, this](const PaintFlags& paint) {
    if (font.drawText(m_canvas, runInfo, point, m_deviceScaleFactor, paint))
      m_paintController.setTextPainted();
  });
}

void GraphicsContext::drawEmphasisMarks(const Font& font,
                                        const TextRunPaintInfo& runInfo,
                                        const AtomicString& mark,
                                        const FloatPoint& point) {
  if (contextDisabled())
    return;

  drawTextPasses([&font, &runInfo, &mark, &point,
                  this](const PaintFlags& paint) {
    font.drawEmphasisMarks(m_canvas, runInfo, mark, point, m_deviceScaleFactor,
                           paint);
  });
}

void GraphicsContext::drawBidiText(
    const Font& font,
    const TextRunPaintInfo& runInfo,
    const FloatPoint& point,
    Font::CustomFontNotReadyAction customFontNotReadyAction) {
  if (contextDisabled())
    return;

  drawTextPasses([&font, &runInfo, &point, customFontNotReadyAction,
                  this](const PaintFlags& paint) {
    if (font.drawBidiText(m_canvas, runInfo, point, customFontNotReadyAction,
                          m_deviceScaleFactor, paint))
      m_paintController.setTextPainted();
  });
}

void GraphicsContext::drawHighlightForText(const Font& font,
                                           const TextRun& run,
                                           const FloatPoint& point,
                                           int h,
                                           const Color& backgroundColor,
                                           int from,
                                           int to) {
  if (contextDisabled())
    return;

  fillRect(font.selectionRectForText(run, point, h, from, to), backgroundColor);
}

void GraphicsContext::drawImage(
    Image* image,
    const FloatRect& dest,
    const FloatRect* srcPtr,
    SkBlendMode op,
    RespectImageOrientationEnum shouldRespectImageOrientation) {
  if (contextDisabled() || !image)
    return;

  const FloatRect src = srcPtr ? *srcPtr : image->rect();

  PaintFlags imagePaint = immutableState()->fillPaint();
  imagePaint.setBlendMode(op);
  imagePaint.setColor(SK_ColorBLACK);
  imagePaint.setFilterQuality(computeFilterQuality(image, dest, src));
  imagePaint.setAntiAlias(shouldAntialias());
  image->draw(m_canvas, imagePaint, dest, src, shouldRespectImageOrientation,
              Image::ClampImageToSourceRect, m_colorBehavior);
  m_paintController.setImagePainted();
}

void GraphicsContext::drawImageRRect(
    Image* image,
    const FloatRoundedRect& dest,
    const FloatRect& srcRect,
    SkBlendMode op,
    RespectImageOrientationEnum respectOrientation) {
  if (contextDisabled() || !image)
    return;

  if (!dest.isRounded()) {
    drawImage(image, dest.rect(), &srcRect, op, respectOrientation);
    return;
  }

  DCHECK(dest.isRenderable());

  const FloatRect visibleSrc = intersection(srcRect, image->rect());
  if (dest.isEmpty() || visibleSrc.isEmpty())
    return;

  PaintFlags imagePaint = immutableState()->fillPaint();
  imagePaint.setBlendMode(op);
  imagePaint.setColor(SK_ColorBLACK);
  imagePaint.setFilterQuality(
      computeFilterQuality(image, dest.rect(), srcRect));
  imagePaint.setAntiAlias(shouldAntialias());

  bool useShader = (visibleSrc == srcRect) &&
                   (respectOrientation == DoNotRespectImageOrientation);
  if (useShader) {
    const SkMatrix localMatrix = SkMatrix::MakeRectToRect(
        visibleSrc, dest.rect(), SkMatrix::kFill_ScaleToFit);
    useShader = image->applyShader(imagePaint, localMatrix, m_colorBehavior);
  }

  if (useShader) {
    // Shader-based fast path.
    m_canvas->drawRRect(dest, imagePaint);
  } else {
    // Clip-based fallback.
    PaintCanvasAutoRestore autoRestore(m_canvas, true);
    m_canvas->clipRRect(dest, imagePaint.isAntiAlias());
    image->draw(m_canvas, imagePaint, dest.rect(), srcRect, respectOrientation,
                Image::ClampImageToSourceRect, m_colorBehavior);
  }

  m_paintController.setImagePainted();
}

SkFilterQuality GraphicsContext::computeFilterQuality(
    Image* image,
    const FloatRect& dest,
    const FloatRect& src) const {
  InterpolationQuality resampling;
  if (printing()) {
    resampling = InterpolationNone;
  } else if (image->currentFrameIsLazyDecoded()) {
    resampling = InterpolationHigh;
  } else {
    resampling = computeInterpolationQuality(
        SkScalarToFloat(src.width()), SkScalarToFloat(src.height()),
        SkScalarToFloat(dest.width()), SkScalarToFloat(dest.height()),
        image->currentFrameIsComplete());

    if (resampling == InterpolationNone) {
      // FIXME: This is to not break tests (it results in the filter bitmap flag
      // being set to true). We need to decide if we respect InterpolationNone
      // being returned from computeInterpolationQuality.
      resampling = InterpolationLow;
    }
  }
  return static_cast<SkFilterQuality>(
      limitInterpolationQuality(*this, resampling));
}

void GraphicsContext::drawTiledImage(Image* image,
                                     const FloatRect& destRect,
                                     const FloatPoint& srcPoint,
                                     const FloatSize& tileSize,
                                     SkBlendMode op,
                                     const FloatSize& repeatSpacing) {
  if (contextDisabled() || !image)
    return;
  image->drawTiledBackground(*this, destRect, srcPoint, tileSize, op,
                             repeatSpacing);
  m_paintController.setImagePainted();
}

void GraphicsContext::drawTiledImage(Image* image,
                                     const FloatRect& dest,
                                     const FloatRect& srcRect,
                                     const FloatSize& tileScaleFactor,
                                     Image::TileRule hRule,
                                     Image::TileRule vRule,
                                     SkBlendMode op) {
  if (contextDisabled() || !image)
    return;

  if (hRule == Image::StretchTile && vRule == Image::StretchTile) {
    // Just do a scale.
    drawImage(image, dest, &srcRect, op);
    return;
  }

  image->drawTiledBorder(*this, dest, srcRect, tileScaleFactor, hRule, vRule,
                         op);
  m_paintController.setImagePainted();
}

void GraphicsContext::drawOval(const SkRect& oval, const PaintFlags& paint) {
  if (contextDisabled())
    return;
  ASSERT(m_canvas);

  m_canvas->drawOval(oval, paint);
}

void GraphicsContext::drawPath(const SkPath& path, const PaintFlags& paint) {
  if (contextDisabled())
    return;
  ASSERT(m_canvas);

  m_canvas->drawPath(path, paint);
}

void GraphicsContext::drawRect(const SkRect& rect, const PaintFlags& paint) {
  if (contextDisabled())
    return;
  ASSERT(m_canvas);

  m_canvas->drawRect(rect, paint);
}

void GraphicsContext::drawRRect(const SkRRect& rrect, const PaintFlags& paint) {
  if (contextDisabled())
    return;
  ASSERT(m_canvas);

  m_canvas->drawRRect(rrect, paint);
}

void GraphicsContext::fillPath(const Path& pathToFill) {
  if (contextDisabled() || pathToFill.isEmpty())
    return;

  drawPath(pathToFill.getSkPath(), immutableState()->fillPaint());
}

void GraphicsContext::fillRect(const FloatRect& rect) {
  if (contextDisabled())
    return;

  drawRect(rect, immutableState()->fillPaint());
}

void GraphicsContext::fillRect(const FloatRect& rect,
                               const Color& color,
                               SkBlendMode xferMode) {
  if (contextDisabled())
    return;

  PaintFlags paint = immutableState()->fillPaint();
  paint.setColor(color.rgb());
  paint.setBlendMode(xferMode);

  drawRect(rect, paint);
}

void GraphicsContext::fillRoundedRect(const FloatRoundedRect& rrect,
                                      const Color& color) {
  if (contextDisabled())
    return;

  if (!rrect.isRounded() || !rrect.isRenderable()) {
    fillRect(rrect.rect(), color);
    return;
  }

  if (color == fillColor()) {
    drawRRect(rrect, immutableState()->fillPaint());
    return;
  }

  PaintFlags paint = immutableState()->fillPaint();
  paint.setColor(color.rgb());

  drawRRect(rrect, paint);
}

namespace {

bool isSimpleDRRect(const FloatRoundedRect& outer,
                    const FloatRoundedRect& inner) {
  // A DRRect is "simple" (i.e. can be drawn as a rrect stroke) if
  //   1) all sides have the same width
  const FloatSize strokeSize =
      inner.rect().minXMinYCorner() - outer.rect().minXMinYCorner();
  if (!WebCoreFloatNearlyEqual(strokeSize.aspectRatio(), 1) ||
      !WebCoreFloatNearlyEqual(strokeSize.width(),
                               outer.rect().maxX() - inner.rect().maxX()) ||
      !WebCoreFloatNearlyEqual(strokeSize.height(),
                               outer.rect().maxY() - inner.rect().maxY()))
    return false;

  // and
  //   2) the inner radii are not constrained
  const FloatRoundedRect::Radii& oRadii = outer.getRadii();
  const FloatRoundedRect::Radii& iRadii = inner.getRadii();
  if (!WebCoreFloatNearlyEqual(oRadii.topLeft().width() - strokeSize.width(),
                               iRadii.topLeft().width()) ||
      !WebCoreFloatNearlyEqual(oRadii.topLeft().height() - strokeSize.height(),
                               iRadii.topLeft().height()) ||
      !WebCoreFloatNearlyEqual(oRadii.topRight().width() - strokeSize.width(),
                               iRadii.topRight().width()) ||
      !WebCoreFloatNearlyEqual(oRadii.topRight().height() - strokeSize.height(),
                               iRadii.topRight().height()) ||
      !WebCoreFloatNearlyEqual(
          oRadii.bottomRight().width() - strokeSize.width(),
          iRadii.bottomRight().width()) ||
      !WebCoreFloatNearlyEqual(
          oRadii.bottomRight().height() - strokeSize.height(),
          iRadii.bottomRight().height()) ||
      !WebCoreFloatNearlyEqual(oRadii.bottomLeft().width() - strokeSize.width(),
                               iRadii.bottomLeft().width()) ||
      !WebCoreFloatNearlyEqual(
          oRadii.bottomLeft().height() - strokeSize.height(),
          iRadii.bottomLeft().height()))
    return false;

  // We also ignore DRRects with a very thick relative stroke (shapes which are
  // mostly filled by the stroke): Skia's stroke outline can diverge
  // significantly from the outer/inner contours in some edge cases, so we fall
  // back to drawDRRect() instead.
  const float kMaxStrokeToSizeRatio = 0.75f;
  if (2 * strokeSize.width() / outer.rect().width() > kMaxStrokeToSizeRatio ||
      2 * strokeSize.height() / outer.rect().height() > kMaxStrokeToSizeRatio)
    return false;

  return true;
}

}  // anonymous namespace

void GraphicsContext::fillDRRect(const FloatRoundedRect& outer,
                                 const FloatRoundedRect& inner,
                                 const Color& color) {
  if (contextDisabled())
    return;
  ASSERT(m_canvas);

  if (!isSimpleDRRect(outer, inner)) {
    if (color == fillColor()) {
      m_canvas->drawDRRect(outer, inner, immutableState()->fillPaint());
    } else {
      PaintFlags paint(immutableState()->fillPaint());
      paint.setColor(color.rgb());
      m_canvas->drawDRRect(outer, inner, paint);
    }

    return;
  }

  // We can draw this as a stroked rrect.
  float strokeWidth = inner.rect().x() - outer.rect().x();
  SkRRect strokeRRect = outer;
  strokeRRect.inset(strokeWidth / 2, strokeWidth / 2);

  PaintFlags strokePaint(immutableState()->fillPaint());
  strokePaint.setColor(color.rgb());
  strokePaint.setStyle(PaintFlags::kStroke_Style);
  strokePaint.setStrokeWidth(strokeWidth);

  m_canvas->drawRRect(strokeRRect, strokePaint);
}

void GraphicsContext::fillEllipse(const FloatRect& ellipse) {
  if (contextDisabled())
    return;

  drawOval(ellipse, immutableState()->fillPaint());
}

void GraphicsContext::strokePath(const Path& pathToStroke) {
  if (contextDisabled() || pathToStroke.isEmpty())
    return;

  drawPath(pathToStroke.getSkPath(), immutableState()->strokePaint());
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth) {
  if (contextDisabled())
    return;

  PaintFlags paint(immutableState()->strokePaint());
  paint.setStrokeWidth(WebCoreFloatToSkScalar(lineWidth));
  // Reset the dash effect to account for the width
  immutableState()->getStrokeData().setupPaintDashPathEffect(&paint, 0);
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

void GraphicsContext::strokeEllipse(const FloatRect& ellipse) {
  if (contextDisabled())
    return;

  drawOval(ellipse, immutableState()->strokePaint());
}

void GraphicsContext::clipRoundedRect(const FloatRoundedRect& rrect,
                                      SkClipOp clipOp,
                                      AntiAliasingMode shouldAntialias) {
  if (contextDisabled())
    return;

  if (!rrect.isRounded()) {
    clipRect(rrect.rect(), shouldAntialias, clipOp);
    return;
  }

  clipRRect(rrect, shouldAntialias, clipOp);
}

void GraphicsContext::clipOut(const Path& pathToClip) {
  if (contextDisabled())
    return;

  // Use const_cast and temporarily toggle the inverse fill type instead of
  // copying the path.
  SkPath& path = const_cast<SkPath&>(pathToClip.getSkPath());
  path.toggleInverseFillType();
  clipPath(path, AntiAliased);
  path.toggleInverseFillType();
}

void GraphicsContext::clipOutRoundedRect(const FloatRoundedRect& rect) {
  if (contextDisabled())
    return;

  clipRoundedRect(rect, SkClipOp::kDifference);
}

void GraphicsContext::clipRect(const SkRect& rect,
                               AntiAliasingMode aa,
                               SkClipOp op) {
  if (contextDisabled())
    return;
  ASSERT(m_canvas);

  m_canvas->clipRect(rect, op, aa == AntiAliased);
}

void GraphicsContext::clipPath(const SkPath& path,
                               AntiAliasingMode aa,
                               SkClipOp op) {
  if (contextDisabled())
    return;
  ASSERT(m_canvas);

  m_canvas->clipPath(path, op, aa == AntiAliased);
}

void GraphicsContext::clipRRect(const SkRRect& rect,
                                AntiAliasingMode aa,
                                SkClipOp op) {
  if (contextDisabled())
    return;
  ASSERT(m_canvas);

  m_canvas->clipRRect(rect, op, aa == AntiAliased);
}

void GraphicsContext::rotate(float angleInRadians) {
  if (contextDisabled())
    return;
  ASSERT(m_canvas);

  m_canvas->rotate(
      WebCoreFloatToSkScalar(angleInRadians * (180.0f / 3.14159265f)));
}

void GraphicsContext::translate(float x, float y) {
  if (contextDisabled())
    return;
  ASSERT(m_canvas);

  if (!x && !y)
    return;

  m_canvas->translate(WebCoreFloatToSkScalar(x), WebCoreFloatToSkScalar(y));
}

void GraphicsContext::scale(float x, float y) {
  if (contextDisabled())
    return;
  ASSERT(m_canvas);

  m_canvas->scale(WebCoreFloatToSkScalar(x), WebCoreFloatToSkScalar(y));
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect) {
  if (contextDisabled())
    return;
  ASSERT(m_canvas);

  sk_sp<SkData> url(SkData::MakeWithCString(link.getString().utf8().data()));
  SkAnnotateRectWithURL(m_canvas, destRect, url.get());
}

void GraphicsContext::setURLFragmentForRect(const String& destName,
                                            const IntRect& rect) {
  if (contextDisabled())
    return;
  ASSERT(m_canvas);

  sk_sp<SkData> skDestName(SkData::MakeWithCString(destName.utf8().data()));
  SkAnnotateLinkToDestination(m_canvas, rect, skDestName.get());
}

void GraphicsContext::setURLDestinationLocation(const String& name,
                                                const IntPoint& location) {
  if (contextDisabled())
    return;
  ASSERT(m_canvas);

  sk_sp<SkData> skName(SkData::MakeWithCString(name.utf8().data()));
  SkAnnotateNamedDestination(
      m_canvas, SkPoint::Make(location.x(), location.y()), skName.get());
}

void GraphicsContext::concatCTM(const AffineTransform& affine) {
  concat(affineTransformToSkMatrix(affine));
}

void GraphicsContext::fillRectWithRoundedHole(
    const FloatRect& rect,
    const FloatRoundedRect& roundedHoleRect,
    const Color& color) {
  if (contextDisabled())
    return;

  PaintFlags paint(immutableState()->fillPaint());
  paint.setColor(color.rgb());
  m_canvas->drawDRRect(SkRRect::MakeRect(rect), roundedHoleRect, paint);
}

void GraphicsContext::adjustLineToPixelBoundaries(FloatPoint& p1,
                                                  FloatPoint& p2,
                                                  float strokeWidth,
                                                  StrokeStyle penStyle) {
  // For odd widths, we add in 0.5 to the appropriate x/y so that the float
  // arithmetic works out.  For example, with a border width of 3, WebKit will
  // pass us (y1+y2)/2, e.g., (50+53)/2 = 103/2 = 51 when we want 51.5.  It is
  // always true that an even width gave us a perfect position, but an odd width
  // gave us a position that is off by exactly 0.5.
  if (penStyle == DottedStroke || penStyle == DashedStroke) {
    if (p1.x() == p2.x()) {
      p1.setY(p1.y() + strokeWidth);
      p2.setY(p2.y() - strokeWidth);
    } else {
      p1.setX(p1.x() + strokeWidth);
      p2.setX(p2.x() - strokeWidth);
    }
  }

  if (static_cast<int>(strokeWidth) % 2) {  // odd
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

sk_sp<SkColorFilter> GraphicsContext::WebCoreColorFilterToSkiaColorFilter(
    ColorFilter colorFilter) {
  switch (colorFilter) {
    case ColorFilterLuminanceToAlpha:
      return SkLumaColorFilter::Make();
    case ColorFilterLinearRGBToSRGB:
      return ColorSpaceUtilities::createColorSpaceFilter(ColorSpaceLinearRGB,
                                                         ColorSpaceDeviceRGB);
    case ColorFilterSRGBToLinearRGB:
      return ColorSpaceUtilities::createColorSpaceFilter(ColorSpaceDeviceRGB,
                                                         ColorSpaceLinearRGB);
    case ColorFilterNone:
      break;
    default:
      ASSERT_NOT_REACHED();
      break;
  }

  return nullptr;
}

}  // namespace blink
