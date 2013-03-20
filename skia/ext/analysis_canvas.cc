// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "skia/ext/analysis_canvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkDraw.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/src/core/SkRasterClip.h"
#include "ui/gfx/rect_conversions.h"

namespace {

// FIXME: Arbitrary numbers. Requires tuning & experimentation.
// Probably requires per-platform tuning; N10 average draw call takes
// 25x as long as Z620.
const int gPictureCostThreshold = 1000;
const int kUnknownExpensiveCost = 500;
const int kUnknownBitmapCost = 1000;

// URI label for a lazily decoded SkPixelRef.
const char kLabelLazyDecoded[] = "lazy";
const int kLabelLazyDecodedLength = 4;

// Estimate of rasterization performance on mid-low-range hardware,
// drawing rectangles with simple paints.
const int kSimpleRectPixelsPerUS = 1000;
const int kComplexRectPixelsPerUS = 100;
const int kSimpleTextCharPerUS = 2;

bool isSolidColorPaint(const SkPaint& paint) {
  SkXfermode::Mode xferMode;

  // getXfermode can return a NULL, but that is handled
  // gracefully by AsMode (NULL turns into kSrcOver mode).
  SkXfermode::AsMode(paint.getXfermode(), &xferMode);

  // Paint is solid color if the following holds:
  // - Alpha is 1.0, style is fill, and there are no special effects
  // - Xfer mode is either kSrc or kSrcOver (kSrcOver is equivalent
  //   to kSrc if source alpha is 1.0, which is already checked).
  return (paint.getAlpha() == 255 &&
          !paint.getShader() &&
          !paint.getLooper() &&
          !paint.getMaskFilter() &&
          !paint.getColorFilter() &&
          paint.getStyle() == SkPaint::kFill_Style &&
          (xferMode == SkXfermode::kSrc_Mode ||
           xferMode == SkXfermode::kSrcOver_Mode));
}

bool isFullQuad(const SkDraw& draw,
                const SkRect& canvasRect,
                const SkRect& drawnRect) {

  // If the transform results in a non-axis aligned
  // rect, then be conservative and return false.
  if (!draw.fMatrix->rectStaysRect())
    return false;

  SkRect drawBitmapRect;
  draw.fBitmap->getBounds(&drawBitmapRect);
  SkRect clipRect = SkRect::Make(draw.fRC->getBounds());
  SkRect deviceRect;
  draw.fMatrix->mapRect(&deviceRect, drawnRect);

  // The drawn rect covers the full canvas, if the following conditions hold:
  // - Clip rect is an actual rectangle.
  // - The rect we're drawing (post-transform) contains the clip rect.
  //   That is, all of clip rect will be colored by the rect.
  // - Clip rect contains the canvas rect.
  //   That is, we're not clipping to a portion of this canvas.
  // - The bitmap into which the draw call happens is at least as
  //   big as the canvas rect
  return draw.fRC->isRect() &&
         deviceRect.contains(clipRect) &&
         clipRect.contains(canvasRect) &&
         drawBitmapRect.contains(canvasRect);
}

bool hasBitmap(const SkPaint& paint) {
  SkShader* shader = paint.getShader();
  return shader &&
      (SkShader::kNone_BitmapType != shader->asABitmap(NULL, NULL, NULL));
}

} // namespace

namespace skia {

AnalysisDevice::AnalysisDevice(const SkBitmap& bm)
  : INHERITED(bm)
  , estimatedCost_(0)
  , isForcedNotSolid_(false)
  , isForcedNotTransparent_(false)
  , isSolidColor_(false)
  , isTransparent_(false) {

}

AnalysisDevice::~AnalysisDevice() {

}

int AnalysisDevice::getEstimatedCost() const {
  return estimatedCost_;
}

bool AnalysisDevice::getColorIfSolid(SkColor* color) const {
  if (isSolidColor_)
    *color = color_;
  return isSolidColor_;
}

bool AnalysisDevice::isTransparent() const {
  return isTransparent_;
}

void AnalysisDevice::setForceNotSolid(bool flag) {
  isForcedNotSolid_ = flag;
  if (isForcedNotSolid_)
    isSolidColor_ = false;
}

void AnalysisDevice::setForceNotTransparent(bool flag) {
  isForcedNotTransparent_ = flag;
  if (isForcedNotTransparent_)
    isTransparent_ = false;
}

void AnalysisDevice::addPixelRefIfLazy(SkPixelRef* pixelRef) {
  if (!pixelRef)
    return;

  uint32_t genID = pixelRef->getGenerationID();

  // If this ID exists (whether it is lazy pixel ref or not),
  // we can return early.
  std::pair<IdSet::iterator, bool> insertionResult =
      existingPixelRefIDs_.insert(genID);
  if (!insertionResult.second)
    return;

  if (pixelRef->getURI() &&
      !strncmp(pixelRef->getURI(),
               kLabelLazyDecoded,
               kLabelLazyDecodedLength)) {
    lazyPixelRefs_.push_back(static_cast<skia::LazyPixelRef*>(pixelRef));
  }
}

void AnalysisDevice::addBitmap(const SkBitmap& bitmap) {
  addPixelRefIfLazy(bitmap.pixelRef());
}

void AnalysisDevice::addBitmapFromPaint(const SkPaint& paint) {
  SkShader* shader = paint.getShader();
  if (shader) {
    SkBitmap bitmap;
    // Check whether the shader is a gradient in order to short-circuit
    // call to asABitmap to prevent generation of bitmaps from
    // gradient shaders, which implement asABitmap.
    if (SkShader::kNone_GradientType == shader->asAGradient(NULL) &&
        SkShader::kNone_BitmapType != shader->asABitmap(&bitmap, NULL, NULL)) {
        addPixelRefIfLazy(bitmap.pixelRef());
    }
  }
}

void AnalysisDevice::consumeLazyPixelRefs(LazyPixelRefList* pixelRefs) {
  DCHECK(pixelRefs);
  DCHECK(pixelRefs->empty());
  lazyPixelRefs_.swap(*pixelRefs);
  existingPixelRefIDs_.clear();
}

void AnalysisDevice::clear(SkColor color) {
  // FIXME: cost here should be simple rect of device size
  estimatedCost_ += kUnknownExpensiveCost;

  isTransparent_ = (!isForcedNotTransparent_ && SkColorGetA(color) == 0);

  if (!isForcedNotSolid_ && SkColorGetA(color) == 255) {
    isSolidColor_ = true;
    color_ = color;
  }
  else {
    isSolidColor_ = false;
  }
}

void AnalysisDevice::drawPaint(const SkDraw&, const SkPaint& paint) {
  estimatedCost_ += kUnknownExpensiveCost;
  if (hasBitmap(paint)) {
    estimatedCost_ += kUnknownBitmapCost;
    addBitmapFromPaint(paint);
  }
  isSolidColor_ = false;
  isTransparent_ = false;
}

void AnalysisDevice::drawPoints(const SkDraw&, SkCanvas::PointMode mode,
                          size_t count, const SkPoint[],
                          const SkPaint& paint) {
  estimatedCost_ += kUnknownExpensiveCost;
  if (hasBitmap(paint)) {
    estimatedCost_ += kUnknownBitmapCost;
    addBitmapFromPaint(paint);
  }
  isSolidColor_ = false;
  isTransparent_ = false;
}

void AnalysisDevice::drawRect(const SkDraw& draw, const SkRect& rect,
                        const SkPaint& paint) {

  // FIXME: if there's a pending image decode & resize, more expensive
  estimatedCost_ += 1 + rect.width() * rect.height() / kSimpleRectPixelsPerUS;
  if (paint.getMaskFilter()) {
    estimatedCost_ += kUnknownExpensiveCost;
  }
  if (hasBitmap(paint)) {
    estimatedCost_ += kUnknownBitmapCost;
    addBitmapFromPaint(paint);
  }

  bool doesCoverCanvas = isFullQuad(draw,
                                    SkRect::MakeWH(width(), height()),
                                    rect);

  SkXfermode::Mode xferMode;
  SkXfermode::AsMode(paint.getXfermode(), &xferMode);

  // This canvas will become transparent if the following holds:
  // - The quad is a full tile quad
  // - We're not in "forced not transparent" mode
  // - Transfer mode is clear (0 color, 0 alpha)
  //
  // If the paint alpha is not 0, or if the transfrer mode is
  // not src, then this canvas will not be transparent.
  //
  // In all other cases, we keep the current transparent value
  if (doesCoverCanvas &&
      !isForcedNotTransparent_ &&
      xferMode == SkXfermode::kClear_Mode) {
    isTransparent_ = true;
  }
  else if (paint.getAlpha() != 0 ||
           xferMode != SkXfermode::kSrc_Mode) {
    isTransparent_ = false;
  }

  // This bitmap is solid if and only if the following holds.
  // Note that this might be overly conservative:
  // - We're not in "forced not solid" mode
  // - Paint is solid color
  // - The quad is a full tile quad
  if (!isForcedNotSolid_ &&
      isSolidColorPaint(paint) &&
      doesCoverCanvas) {
    isSolidColor_ = true;
    color_ = paint.getColor();
  }
  else {
    isSolidColor_ = false;
  }
}

void AnalysisDevice::drawOval(const SkDraw&, const SkRect& oval,
                        const SkPaint& paint) {
  estimatedCost_ += kUnknownExpensiveCost;
  if (hasBitmap(paint)) {
    estimatedCost_ += kUnknownBitmapCost;
    addBitmapFromPaint(paint);
  }
  isSolidColor_ = false;
  isTransparent_ = false;
}

void AnalysisDevice::drawPath(const SkDraw&, const SkPath& path,
                        const SkPaint& paint,
                        const SkMatrix* prePathMatrix,
                        bool pathIsMutable ) {
  // On Z620, every antialiased path costs us about 300us.
  // We've only seen this in practice on filled paths, but
  // we expect it to apply to all path stroking modes.
  if (paint.getMaskFilter()) {
    estimatedCost_ += 300;
  }
  // FIXME: horrible overestimate if the path is stroked instead of filled
  estimatedCost_ += 1 + path.getBounds().width() *
      path.getBounds().height() / kSimpleRectPixelsPerUS;
  if (hasBitmap(paint)) {
    estimatedCost_ += kUnknownBitmapCost;
    addBitmapFromPaint(paint);
  }
  isSolidColor_ = false;
  isTransparent_ = false;
}

void AnalysisDevice::drawBitmap(const SkDraw&, const SkBitmap& bitmap,
                          const SkIRect* srcRectOrNull,
                          const SkMatrix& matrix, const SkPaint& paint) {
  estimatedCost_ += kUnknownExpensiveCost;
  //DCHECK(hasBitmap(paint));
  estimatedCost_ += kUnknownBitmapCost;
  isSolidColor_ = false;
  isTransparent_ = false;
  addBitmap(bitmap);
}

void AnalysisDevice::drawSprite(const SkDraw&, const SkBitmap& bitmap,
                          int x, int y, const SkPaint& paint) {
  estimatedCost_ += kUnknownExpensiveCost;
  //DCHECK(hasBitmap(paint));
  estimatedCost_ += kUnknownBitmapCost;
  isSolidColor_ = false;
  isTransparent_ = false;
  addBitmap(bitmap);
}

void AnalysisDevice::drawBitmapRect(const SkDraw& draw, const SkBitmap& bitmap,
                              const SkRect* srcOrNull, const SkRect& dst,
                              const SkPaint& paint) {
  // FIXME: we also accumulate cost from drawRect()
  estimatedCost_ += 1 + dst.width() * dst.height() / kComplexRectPixelsPerUS;
  //DCHECK(hasBitmap(paint));
  estimatedCost_ += kUnknownBitmapCost;

  // Call drawRect to determine transparency,
  // but reset solid color to false.
  drawRect(draw, dst, paint);
  isSolidColor_ = false;
  addBitmap(bitmap);
}


void AnalysisDevice::drawText(const SkDraw&, const void* text, size_t len,
                        SkScalar x, SkScalar y, const SkPaint& paint) {
  estimatedCost_ += 1 + len / kSimpleTextCharPerUS;
  if (hasBitmap(paint)) {
    estimatedCost_ += kUnknownBitmapCost;
    addBitmapFromPaint(paint);
  }
  isSolidColor_ = false;
  isTransparent_ = false;
}

void AnalysisDevice::drawPosText(const SkDraw& draw, const void* text,
                           size_t len,
                           const SkScalar pos[], SkScalar constY,
                           int scalarsPerPos, const SkPaint& paint) {
  // FIXME: On Z620, every glyph cache miss costs us about 10us.
  // We don't have a good mechanism for predicting glyph cache misses.
  estimatedCost_ += 1 + len / kSimpleTextCharPerUS;
  if (hasBitmap(paint)) {
    estimatedCost_ += kUnknownBitmapCost;
    addBitmapFromPaint(paint);
  }
  isSolidColor_ = false;
  isTransparent_ = false;
}

void AnalysisDevice::drawTextOnPath(const SkDraw&, const void* text,
                              size_t len,
                              const SkPath& path, const SkMatrix* matrix,
                              const SkPaint& paint) {
  estimatedCost_ += 1 + len / kSimpleTextCharPerUS;
  if (hasBitmap(paint)) {
    estimatedCost_ += kUnknownBitmapCost;
    addBitmapFromPaint(paint);
  }
  isSolidColor_ = false;
  isTransparent_ = false;
}

#ifdef SK_BUILD_FOR_ANDROID
void AnalysisDevice::drawPosTextOnPath(const SkDraw& draw, const void* text,
                                 size_t len,
                                 const SkPoint pos[], const SkPaint& paint,
                                 const SkPath& path, const SkMatrix* matrix) {
  estimatedCost_ += 1 + len / kSimpleTextCharPerUS;
  if (hasBitmap(paint)) {
    estimatedCost_ += kUnknownBitmapCost;
    addBitmapFromPaint(paint);
  }
  isSolidColor_ = false;
  isTransparent_ = false;
}
#endif

void AnalysisDevice::drawVertices(const SkDraw&, SkCanvas::VertexMode,
                            int vertexCount,
                            const SkPoint verts[], const SkPoint texs[],
                            const SkColor colors[], SkXfermode* xmode,
                            const uint16_t indices[], int indexCount,
                            const SkPaint& paint) {
  estimatedCost_ += kUnknownExpensiveCost;
  if (hasBitmap(paint)) {
    estimatedCost_ += kUnknownBitmapCost;
    addBitmapFromPaint(paint);
  }
  isSolidColor_ = false;
  isTransparent_ = false;
}

void AnalysisDevice::drawDevice(const SkDraw&, SkDevice*, int x, int y,
                          const SkPaint& paint) {
  estimatedCost_ += kUnknownExpensiveCost;
  if (hasBitmap(paint))
    estimatedCost_ += kUnknownBitmapCost;
  isSolidColor_ = false;
  isTransparent_ = false;
}


const int AnalysisCanvas::kNoLayer = -1;

AnalysisCanvas::AnalysisCanvas(AnalysisDevice* device)
  : INHERITED(device)
  , savedStackSize_(0)
  , forceNotSolidStackLevel_(kNoLayer)
  , forceNotTransparentStackLevel_(kNoLayer) {
}

AnalysisCanvas::~AnalysisCanvas() {
}


bool AnalysisCanvas::isCheap() const {
  return getEstimatedCost() < gPictureCostThreshold;
}

bool AnalysisCanvas::getColorIfSolid(SkColor* color) const {
  return (static_cast<AnalysisDevice*>(getDevice()))->getColorIfSolid(color);
}

bool AnalysisCanvas::isTransparent() const {
  return (static_cast<AnalysisDevice*>(getDevice()))->isTransparent();
}

int AnalysisCanvas::getEstimatedCost() const {
  return (static_cast<AnalysisDevice*>(getDevice()))->getEstimatedCost();
}

void AnalysisCanvas::consumeLazyPixelRefs(LazyPixelRefList* pixelRefs) {
  static_cast<AnalysisDevice*>(getDevice())->consumeLazyPixelRefs(pixelRefs);
}

bool AnalysisCanvas::clipRect(const SkRect& rect, SkRegion::Op op,
                        bool doAA) {
  return INHERITED::clipRect(rect, op, doAA);
}

bool AnalysisCanvas::clipPath(const SkPath& path, SkRegion::Op op,
                        bool doAA) {
  // clipPaths can make our calls to isFullQuad invalid (ie have false
  // positives). As a precaution, force the setting to be non-solid
  // and non-transparent until we pop this 
  if (forceNotSolidStackLevel_ == kNoLayer) {
    forceNotSolidStackLevel_ = savedStackSize_;
    (static_cast<AnalysisDevice*>(getDevice()))->setForceNotSolid(true);
  }
  if (forceNotTransparentStackLevel_ == kNoLayer) {
    forceNotTransparentStackLevel_ = savedStackSize_;
    (static_cast<AnalysisDevice*>(getDevice()))->setForceNotTransparent(true);
  }

  return INHERITED::clipRect(path.getBounds(), op, doAA);
}

bool AnalysisCanvas::clipRRect(const SkRRect& rrect, SkRegion::Op op,
                         bool doAA) {
  // clipRRect can make our calls to isFullQuad invalid (ie have false
  // positives). As a precaution, force the setting to be non-solid
  // and non-transparent until we pop this 
  if (forceNotSolidStackLevel_ == kNoLayer) {
    forceNotSolidStackLevel_ = savedStackSize_;
    (static_cast<AnalysisDevice*>(getDevice()))->setForceNotSolid(true);
  }
  if (forceNotTransparentStackLevel_ == kNoLayer) {
    forceNotTransparentStackLevel_ = savedStackSize_;
    (static_cast<AnalysisDevice*>(getDevice()))->setForceNotTransparent(true);
  }

  return INHERITED::clipRect(rrect.getBounds(), op, doAA);
}

int AnalysisCanvas::save(SkCanvas::SaveFlags flags) {
  ++savedStackSize_;
  return INHERITED::save(flags);
}

int AnalysisCanvas::saveLayer(const SkRect* bounds, const SkPaint* paint, 
                              SkCanvas::SaveFlags flags) {
  ++savedStackSize_;

  // If after we draw to the saved layer, we have to blend with the current
  // layer, then we can conservatively say that the canvas will not be of
  // solid color.
  if ((paint && !isSolidColorPaint(*paint)) ||
      (bounds && !bounds->contains(
          SkRect::MakeWH(getDevice()->width(), getDevice()->height())))) {
    if (forceNotSolidStackLevel_ == kNoLayer) {
      forceNotSolidStackLevel_ = savedStackSize_;
      (static_cast<AnalysisDevice*>(getDevice()))->setForceNotSolid(true);
    }
  }

  // If after we draw to the save layer, we have to blend with the current
  // layer using any part of the current layer's alpha, then we can
  // conservatively say that the canvas will not be transparent.
  SkXfermode::Mode xferMode = SkXfermode::kSrc_Mode;
  if (paint)
    SkXfermode::AsMode(paint->getXfermode(), &xferMode);
  if (xferMode != SkXfermode::kSrc_Mode) {
    if (forceNotTransparentStackLevel_ == kNoLayer) {
      forceNotTransparentStackLevel_ = savedStackSize_;
      (static_cast<AnalysisDevice*>(getDevice()))->setForceNotTransparent(true);
    }
  }

  // Actually saving a layer here could cause a new bitmap to be created
  // and real rendering to occur.
  int count = INHERITED::save(flags);
  if (bounds) {
    INHERITED::clipRectBounds(bounds, flags, NULL);
  }
  return count;
}

void AnalysisCanvas::restore() {
  INHERITED::restore();

  DCHECK(savedStackSize_);
  if (savedStackSize_) {
    --savedStackSize_;
    if (savedStackSize_ < forceNotSolidStackLevel_) {
      (static_cast<AnalysisDevice*>(getDevice()))->setForceNotSolid(false);
      forceNotSolidStackLevel_ = kNoLayer;
    }
    if (savedStackSize_ < forceNotTransparentStackLevel_) {
      (static_cast<AnalysisDevice*>(getDevice()))->setForceNotTransparent(false);
      forceNotTransparentStackLevel_ = kNoLayer;
    }
  }
}

}  // namespace skia


