// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/opacity_filter_canvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTLazy.h"

namespace skia {

OpacityFilterCanvas::OpacityFilterCanvas(SkCanvas* canvas,
                                         float opacity,
                                         bool disable_image_filtering)
    : INHERITED(canvas),
      alpha_(SkScalarRoundToInt(opacity * 255)),
      disable_image_filtering_(disable_image_filtering) { }

void OpacityFilterCanvas::onFilterPaint(SkPaint* paint, Type) const {
  if (alpha_ < 255)
    paint->setAlpha(alpha_);

  if (disable_image_filtering_)
    paint->setFilterQuality(kNone_SkFilterQuality);
}

void OpacityFilterCanvas::onDrawPicture(const SkPicture* picture,
                                        const SkMatrix* matrix,
                                        const SkPaint* paint) {
  SkTLazy<SkPaint> filteredPaint;
  if (paint) {
    this->onFilterPaint(filteredPaint.set(*paint), kPicture_Type);
  }

  // Unfurl pictures in order to filter nested paints.
  this->SkCanvas::onDrawPicture(picture, matrix, filteredPaint.getMaybeNull());
}

}  // namespace skia
