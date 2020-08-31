// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_BENCHMARKING_CANVAS_H_
#define SKIA_EXT_BENCHMARKING_CANVAS_H_

#include <stddef.h>

#include "base/values.h"
#include "third_party/skia/include/utils/SkNWayCanvas.h"

namespace skia {

class SK_API BenchmarkingCanvas : public SkNWayCanvas {
public:
  BenchmarkingCanvas(SkCanvas* canvas);
  ~BenchmarkingCanvas() override;

  // Returns the number of draw commands executed on this canvas.
  size_t CommandCount() const;

  // Returns the list of executed draw commands.
  const base::ListValue& Commands() const;

  // Return the recorded render time (milliseconds) for a draw command index.
  double GetTime(size_t index);

protected:
  // SkCanvas overrides
  void willSave() override;
  SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec&) override;
  void willRestore() override;

  void didConcat44(const SkM44&) override;
  void didConcat(const SkMatrix&) override;
  void didScale(SkScalar, SkScalar) override;
  void didTranslate(SkScalar, SkScalar) override;
  void didSetMatrix(const SkMatrix&) override;

  void onClipRect(const SkRect&, SkClipOp, ClipEdgeStyle) override;
  void onClipRRect(const SkRRect&, SkClipOp, ClipEdgeStyle) override;
  void onClipPath(const SkPath&, SkClipOp, ClipEdgeStyle) override;
  void onClipRegion(const SkRegion&, SkClipOp) override;

  void onDrawPaint(const SkPaint&) override;
  void onDrawPoints(PointMode, size_t count, const SkPoint pts[],
                    const SkPaint&) override;
  void onDrawRect(const SkRect&, const SkPaint&) override;
  void onDrawOval(const SkRect&, const SkPaint&) override;
  void onDrawRRect(const SkRRect&, const SkPaint&) override;
  void onDrawDRRect(const SkRRect&, const SkRRect&, const SkPaint&) override;
  void onDrawPath(const SkPath&, const SkPaint&) override;

  void onDrawPicture(const SkPicture*, const SkMatrix*, const SkPaint*) override;

  void onDrawImage(const SkImage*, SkScalar left, SkScalar top, const SkPaint*) override;
  void onDrawImageRect(const SkImage*, const SkRect* src, const SkRect& dst,
                       const SkPaint*, SrcRectConstraint) override;

  void onDrawTextBlob(const SkTextBlob* blob, SkScalar x, SkScalar y,
                      const SkPaint& paint) override;

private:
  typedef SkNWayCanvas INHERITED;

  class AutoOp;

  base::ListValue op_records_;
};

}
#endif // SKIA_EXT_BENCHMARKING_CANVAS_H
