// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_ANALYSIS_CANVAS_H_
#define SKIA_EXT_ANALYSIS_CANVAS_H_

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace skia {

class AnalysisDevice;

// Does not render anything, but gathers statistics about a region
// (specified as a clip rectangle) of an SkPicture as the picture is
// played back through it.
// To use: create a SkBitmap with kNo_Config, create an AnalysisDevice
// using that bitmap, and create an AnalysisCanvas using the device.
// Play a picture into the canvas, and then check isCheap().
class SK_API AnalysisCanvas : public SkCanvas {
 public:
  AnalysisCanvas(AnalysisDevice*);
  virtual ~AnalysisCanvas();

  // Returns true if the estimated cost of drawing is below an
  // arbitrary threshold.
  bool isCheap() const;

  // Returns the estimated cost of drawing, in arbitrary units.
  int getEstimatedCost() const;

  virtual bool clipRect(const SkRect& rect,
                        SkRegion::Op op = SkRegion::kIntersect_Op,
                        bool doAntiAlias = false) OVERRIDE;
  virtual bool clipPath(const SkPath& path,
                        SkRegion::Op op = SkRegion::kIntersect_Op,
                        bool doAntiAlias = false) OVERRIDE;
  virtual bool clipRRect(const SkRRect& rrect,
                         SkRegion::Op op = SkRegion::kIntersect_Op,
                         bool doAntiAlias = false) OVERRIDE;

  virtual int saveLayer(const SkRect* bounds, const SkPaint*,
                              SkCanvas::SaveFlags flags) OVERRIDE;
 private:
  typedef SkCanvas INHERITED;
};

class SK_API AnalysisDevice : public SkDevice {
 public:
  AnalysisDevice(const SkBitmap& bm);
  virtual ~AnalysisDevice();

  int getEstimatedCost() const;

 protected:
  virtual void clear(SkColor color) OVERRIDE;
  virtual void drawPaint(const SkDraw&, const SkPaint& paint) OVERRIDE;
  virtual void drawPoints(const SkDraw&, SkCanvas::PointMode mode,
                          size_t count, const SkPoint[],
                          const SkPaint& paint) OVERRIDE;
  virtual void drawRect(const SkDraw&, const SkRect& r,
                        const SkPaint& paint) OVERRIDE;
  virtual void drawOval(const SkDraw&, const SkRect& oval,
                        const SkPaint& paint) OVERRIDE;
  virtual void drawPath(const SkDraw&, const SkPath& path,
                        const SkPaint& paint,
                        const SkMatrix* prePathMatrix = NULL,
                        bool pathIsMutable = false) OVERRIDE;
  virtual void drawBitmap(const SkDraw&, const SkBitmap& bitmap,
                          const SkIRect* srcRectOrNull,
                          const SkMatrix& matrix, const SkPaint& paint)
                          OVERRIDE;
  virtual void drawSprite(const SkDraw&, const SkBitmap& bitmap,
                          int x, int y, const SkPaint& paint) OVERRIDE;
  virtual void drawBitmapRect(const SkDraw&, const SkBitmap&,
                              const SkRect* srcOrNull, const SkRect& dst,
                              const SkPaint& paint) OVERRIDE;
  virtual void drawText(const SkDraw&, const void* text, size_t len,
                        SkScalar x, SkScalar y, const SkPaint& paint)
                        OVERRIDE;
  virtual void drawPosText(const SkDraw& draw, const void* text, size_t len,
                           const SkScalar pos[], SkScalar constY,
                           int scalarsPerPos, const SkPaint& paint) OVERRIDE;
  virtual void drawTextOnPath(const SkDraw&, const void* text, size_t len,
                              const SkPath& path, const SkMatrix* matrix,
                              const SkPaint& paint) OVERRIDE;
#ifdef SK_BUILD_FOR_ANDROID
  virtual void drawPosTextOnPath(const SkDraw& draw, const void* text,
                                 size_t len,
                                 const SkPoint pos[], const SkPaint& paint,
                                 const SkPath& path, const SkMatrix* matrix)
                                 OVERRIDE;
#endif
  virtual void drawVertices(const SkDraw&, SkCanvas::VertexMode,
                            int vertexCount,
                            const SkPoint verts[], const SkPoint texs[],
                            const SkColor colors[], SkXfermode* xmode,
                            const uint16_t indices[], int indexCount,
                            const SkPaint& paint) OVERRIDE;
  virtual void drawDevice(const SkDraw&, SkDevice*, int x, int y,
                          const SkPaint&) OVERRIDE;

  int estimatedCost_;

 private:
  typedef SkDevice INHERITED;
};

}  // namespace skia

#endif  // SKIA_EXT_ANALYSIS_CANVAS_H_

