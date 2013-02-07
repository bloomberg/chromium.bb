// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "skia/ext/analysis_canvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkDraw.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/rect_conversions.h"

namespace {

// FIXME: Arbitrary number. Requires tuning & experimentation.
// Probably requires per-platform tuning; N10 average draw call takes
// 25x as long as Z620.
const int gPictureCostThreshold = 1000;

}

namespace skia {

AnalysisDevice::AnalysisDevice(const SkBitmap& bm)
  : INHERITED(bm)
  , estimatedCost_(0) {

}

AnalysisDevice::~AnalysisDevice() {

}

int AnalysisDevice::getEstimatedCost() const {
  return estimatedCost_;
}

void AnalysisDevice::clear(SkColor color) {
    ++estimatedCost_;
}

void AnalysisDevice::drawPaint(const SkDraw&, const SkPaint& paint) {
  ++estimatedCost_;
}

void AnalysisDevice::drawPoints(const SkDraw&, SkCanvas::PointMode mode,
                          size_t count, const SkPoint[],
                          const SkPaint& paint) {
  ++estimatedCost_;
}

void AnalysisDevice::drawRect(const SkDraw&, const SkRect& r,
                        const SkPaint& paint) {
  // FIXME: if there's a pending image decode & resize, more expensive
  if (paint.getMaskFilter()) {
    estimatedCost_ += 300;
  }
  ++estimatedCost_;
}

void AnalysisDevice::drawOval(const SkDraw&, const SkRect& oval,
                        const SkPaint& paint) {
  ++estimatedCost_;
}

void AnalysisDevice::drawPath(const SkDraw&, const SkPath& path,
                        const SkPaint& paint,
                        const SkMatrix* prePathMatrix ,
                        bool pathIsMutable ) {
  // On Z620, every antialiased path costs us about 300us.
  // We've only seen this in practice on filled paths, but
  // we expect it to apply to all path stroking modes.
  if (paint.getMaskFilter()) {
    estimatedCost_ += 300;
  }
  ++estimatedCost_;
}

void AnalysisDevice::drawBitmap(const SkDraw&, const SkBitmap& bitmap,
                          const SkIRect* srcRectOrNull,
                          const SkMatrix& matrix, const SkPaint& paint)
                          {
  ++estimatedCost_;
}

void AnalysisDevice::drawSprite(const SkDraw&, const SkBitmap& bitmap,
                          int x, int y, const SkPaint& paint) {
  ++estimatedCost_;
}

void AnalysisDevice::drawBitmapRect(const SkDraw&, const SkBitmap&,
                              const SkRect* srcOrNull, const SkRect& dst,
                              const SkPaint& paint) {
  ++estimatedCost_;
}


void AnalysisDevice::drawText(const SkDraw&, const void* text, size_t len,
                        SkScalar x, SkScalar y, const SkPaint& paint)
                        {
  ++estimatedCost_;
}

void AnalysisDevice::drawPosText(const SkDraw& draw, const void* text, size_t len,
                           const SkScalar pos[], SkScalar constY,
                           int scalarsPerPos, const SkPaint& paint) {
  // FIXME: On Z620, every glyph cache miss costs us about 10us.
  // We don't have a good mechanism for predicting glyph cache misses.
  ++estimatedCost_;
}

void AnalysisDevice::drawTextOnPath(const SkDraw&, const void* text, size_t len,
                              const SkPath& path, const SkMatrix* matrix,
                              const SkPaint& paint) {
  ++estimatedCost_;
}

#ifdef SK_BUILD_FOR_ANDROID
void AnalysisDevice::drawPosTextOnPath(const SkDraw& draw, const void* text,
                                 size_t len,
                                 const SkPoint pos[], const SkPaint& paint,
                                 const SkPath& path, const SkMatrix* matrix)
                                 {
  ++estimatedCost_;
}
#endif

void AnalysisDevice::drawVertices(const SkDraw&, SkCanvas::VertexMode,
                            int vertexCount,
                            const SkPoint verts[], const SkPoint texs[],
                            const SkColor colors[], SkXfermode* xmode,
                            const uint16_t indices[], int indexCount,
                            const SkPaint& paint) {
  ++estimatedCost_;
}

void AnalysisDevice::drawDevice(const SkDraw&, SkDevice*, int x, int y,
                          const SkPaint&) {
  ++estimatedCost_;
}




AnalysisCanvas::AnalysisCanvas(AnalysisDevice* device)
  : INHERITED(device) {

}

AnalysisCanvas::~AnalysisCanvas() {
}


bool AnalysisCanvas::isCheap() const {
  return getEstimatedCost() < gPictureCostThreshold;
}

int AnalysisCanvas::getEstimatedCost() const {
  return (static_cast<AnalysisDevice*>(getDevice()))->getEstimatedCost();
}


bool AnalysisCanvas::clipRect(const SkRect& rect, SkRegion::Op op,
                        bool doAA) {
  return INHERITED::clipRect(rect, op, doAA);
}

bool AnalysisCanvas::clipPath(const SkPath& path, SkRegion::Op op,
                        bool doAA) {
  return INHERITED::clipRect(path.getBounds(), op, doAA);
}

bool AnalysisCanvas::clipRRect(const SkRRect& rrect, SkRegion::Op op,
                         bool doAA) {
  return INHERITED::clipRect(rrect.getBounds(), op, doAA);
}

int AnalysisCanvas::saveLayer(const SkRect* bounds, const SkPaint*, 
                              SkCanvas::SaveFlags flags) {
  // Actually saving a layer here could cause a new bitmap to be created
  // and real rendering to occur.
  int count = SkCanvas::save(flags);
  if (bounds) {
    INHERITED::clipRectBounds(bounds, flags, NULL);
  }
  return count;
}

}  // namespace skia


