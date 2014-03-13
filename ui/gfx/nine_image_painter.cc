// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/nine_image_painter.h"

#include <limits>

#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/scoped_canvas.h"

namespace gfx {

namespace {

// Stretches the given image over the specified canvas area.
void Fill(Canvas* c,
          const ImageSkia& i,
          int x,
          int y,
          int w,
          int h,
          const SkPaint& paint) {
  c->DrawImageInt(i, 0, 0, i.width(), i.height(), x, y, w, h, false, paint);
}

}  // namespace

NineImagePainter::NineImagePainter(const std::vector<ImageSkia>& images) {
  DCHECK_EQ(arraysize(images_), images.size());
  for (size_t i = 0; i < arraysize(images_); ++i)
    images_[i] = images[i];
}

NineImagePainter::NineImagePainter(const ImageSkia& image,
                                   const Insets& insets) {
  DCHECK_GE(image.width(), insets.width());
  DCHECK_GE(image.height(), insets.height());

  // Extract subsets of the original image to match the |images_| format.
  const int x[] =
      { 0, insets.left(), image.width() - insets.right(), image.width() };
  const int y[] =
      { 0, insets.top(), image.height() - insets.bottom(), image.height() };

  for (size_t j = 0; j < 3; ++j) {
    for (size_t i = 0; i < 3; ++i) {
      images_[i + j * 3] = ImageSkiaOperations::ExtractSubset(image,
          Rect(x[i], y[j], x[i + 1] - x[i], y[j + 1] - y[j]));
    }
  }
}

NineImagePainter::~NineImagePainter() {
}

bool NineImagePainter::IsEmpty() const {
  return images_[0].isNull();
}

Size NineImagePainter::GetMinimumSize() const {
  return IsEmpty() ? Size() : Size(
      images_[0].width() + images_[1].width() + images_[2].width(),
      images_[0].height() + images_[3].height() + images_[6].height());
}

void NineImagePainter::Paint(Canvas* canvas, const Rect& bounds) {
  // When no alpha value is specified, use default value of 100% opacity.
  Paint(canvas, bounds, std::numeric_limits<uint8>::max());
}


void NineImagePainter::Paint(Canvas* canvas,
                             const Rect& bounds,
                             const uint8 alpha) {
  if (IsEmpty())
    return;

  ScopedCanvas scoped_canvas(canvas);
  canvas->Translate(bounds.OffsetFromOrigin());

  SkPaint paint;
  paint.setAlpha(alpha);

  // In case the corners and edges don't all have the same width/height, we draw
  // the center first, and extend it out in all directions to the edges of the
  // images with the smallest widths/heights.  This way there will be no
  // unpainted areas, though some corners or edges might overlap the center.
  int w = bounds.width();
  int i0w = images_[0].width();
  int i2w = images_[2].width();
  int i3w = images_[3].width();
  int i5w = images_[5].width();
  int i6w = images_[6].width();
  int i8w = images_[8].width();
  int i4x = std::min(std::min(i0w, i3w), i6w);
  int i4w = w - i4x - std::min(std::min(i2w, i5w), i8w);
  int h = bounds.height();
  int i0h = images_[0].height();
  int i1h = images_[1].height();
  int i2h = images_[2].height();
  int i6h = images_[6].height();
  int i7h = images_[7].height();
  int i8h = images_[8].height();
  int i4y = std::min(std::min(i0h, i1h), i2h);
  int i4h = h - i4y - std::min(std::min(i6h, i7h), i8h);
  if (!images_[4].isNull())
    Fill(canvas, images_[4], i4x, i4y, i4w, i4h, paint);
  canvas->DrawImageInt(images_[0], 0, 0, paint);
  Fill(canvas, images_[1], i0w, 0, w - i0w - i2w, i1h, paint);
  canvas->DrawImageInt(images_[2], w - i2w, 0, paint);
  Fill(canvas, images_[3], 0, i0h, i3w, h - i0h - i6h, paint);
  Fill(canvas, images_[5], w - i5w, i2h, i5w, h - i2h - i8h, paint);
  canvas->DrawImageInt(images_[6], 0, h - i6h, paint);
  Fill(canvas, images_[7], i6w, h - i7h, w - i6w - i8w, i7h, paint);
  canvas->DrawImageInt(images_[8], w - i8w, h - i8h, paint);
}

}  // namespace gfx
