// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/discardable_image_utils.h"

#include <algorithm>

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/utils/SkNWayCanvas.h"

namespace skia {

namespace {

class DiscardableImageSet {
 public:
  DiscardableImageSet(std::vector<DiscardableImageUtils::PositionImage>* images)
      : images_(images) {}

  void Add(const SkImage* image,
           const SkRect& rect,
           const SkMatrix& matrix,
           SkFilterQuality filter_quality) {
    // We should only be saving discardable pixel refs.
    SkASSERT(image->isLazyGenerated());

    DiscardableImageUtils::PositionImage position_image;
    position_image.image = image;
    position_image.image_rect = rect;
    position_image.matrix = matrix;
    position_image.filter_quality = filter_quality;
    images_->push_back(position_image);
  }

 private:
  std::vector<DiscardableImageUtils::PositionImage>* images_;
};

SkRect MapRect(const SkMatrix& matrix, const SkRect& src) {
  SkRect dst;
  matrix.mapRect(&dst, src);
  return dst;
}

class GatherDiscardableImageCanvas : public SkNWayCanvas {
 public:
  GatherDiscardableImageCanvas(int width,
                               int height,
                               DiscardableImageSet* image_set)
      : SkNWayCanvas(width, height),
        image_set_(image_set),
        canvas_bounds_(SkRect::MakeIWH(width, height)) {}

 protected:
  // we need to "undo" the behavio of SkNWayCanvas, which will try to forward
  // it.
  void onDrawPicture(const SkPicture* picture,
                     const SkMatrix* matrix,
                     const SkPaint* paint) override {
    SkCanvas::onDrawPicture(picture, matrix, paint);
  }

  void onDrawImage(const SkImage* image,
                   SkScalar x,
                   SkScalar y,
                   const SkPaint* paint) override {
    const SkMatrix& ctm = this->getTotalMatrix();
    AddImage(image, MapRect(ctm, SkRect::MakeXYWH(x, y, image->width(),
                                                  image->height())),
             ctm, paint);
  }

  void onDrawImageRect(const SkImage* image,
                       const SkRect* src,
                       const SkRect& dst,
                       const SkPaint* paint,
                       SrcRectConstraint) override {
    const SkMatrix& ctm = this->getTotalMatrix();
    SkRect src_storage;
    if (!src) {
      src_storage = SkRect::MakeIWH(image->width(), image->height());
      src = &src_storage;
    }
    SkMatrix matrix;
    matrix.setRectToRect(*src, dst, SkMatrix::kFill_ScaleToFit);
    matrix.postConcat(ctm);
    AddImage(image, MapRect(ctm, dst), matrix, paint);
  }

  void onDrawImageNine(const SkImage* image,
                       const SkIRect& center,
                       const SkRect& dst,
                       const SkPaint* paint) override {
    AddImage(image, dst, this->getTotalMatrix(), paint);
  }

 private:
  DiscardableImageSet* image_set_;
  const SkRect canvas_bounds_;

  void AddImage(const SkImage* image,
                const SkRect& rect,
                const SkMatrix& matrix,
                const SkPaint* paint) {
    if (rect.intersects(canvas_bounds_) && image->isLazyGenerated()) {
      SkFilterQuality filter_quality = kNone_SkFilterQuality;
      if (paint) {
        filter_quality = paint->getFilterQuality();
      }
      image_set_->Add(image, rect, matrix, filter_quality);
    }
  }
};

}  // namespace

void DiscardableImageUtils::GatherDiscardableImages(
    SkPicture* picture,
    std::vector<PositionImage>* images) {
  images->clear();
  DiscardableImageSet image_set(images);

  SkIRect picture_ibounds = picture->cullRect().roundOut();
  // Use right/bottom as the size so that we don't need a translate and, as a
  // result, the information is returned relative to the picture's origin.
  GatherDiscardableImageCanvas canvas(picture_ibounds.right(),
                                      picture_ibounds.bottom(), &image_set);

  canvas.drawPicture(picture);
}

}  // namespace skia
