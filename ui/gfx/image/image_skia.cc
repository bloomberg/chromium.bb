// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia.h"

#include <limits>
#include <cmath>

#include "base/logging.h"
#include "ui/gfx/size.h"

namespace gfx {

namespace internal {

// A helper class such that ImageSkia can be cheaply copied. ImageSkia holds a
// refptr instance of ImageSkiaStorage, which in turn holds all of ImageSkia's
// information.
class ImageSkiaStorage : public base::RefCounted<ImageSkiaStorage> {
 public:
  ImageSkiaStorage() {
  }

  void AddBitmap(const SkBitmap& bitmap) {
    bitmaps_.push_back(bitmap);
  }

  const std::vector<SkBitmap>& bitmaps() const { return bitmaps_; }

  void set_size(const gfx::Size& size) { size_ = size; }
  const gfx::Size& size() const { return size_; }

 private:
  ~ImageSkiaStorage() {
  }

  // Bitmaps at different densities.
  std::vector<SkBitmap> bitmaps_;

  // Size of the image in DIP.
  gfx::Size size_;

  friend class base::RefCounted<ImageSkiaStorage>;
};

}  // internal

ImageSkia::ImageSkia() : storage_(NULL) {
}

ImageSkia::ImageSkia(const SkBitmap& bitmap) {
  Init(bitmap);
}

ImageSkia::ImageSkia(const SkBitmap& bitmap, float dip_scale_factor) {
  Init(bitmap, dip_scale_factor);
}

ImageSkia::ImageSkia(const ImageSkia& other) : storage_(other.storage_) {
}

ImageSkia& ImageSkia::operator=(const ImageSkia& other) {
  storage_ = other.storage_;
  return *this;
}

ImageSkia& ImageSkia::operator=(const SkBitmap& other) {
  Init(other);
  return *this;
}

ImageSkia::operator SkBitmap&() const {
  if (isNull()) {
    // TODO(pkotwicz): This is temporary till conversion to gfx::ImageSkia is
    // done.
    // Static null bitmap such that we are not returning a temporary.
    static SkBitmap* null_bitmap = new SkBitmap();
    return *null_bitmap;
  }

  return const_cast<SkBitmap&>(storage_->bitmaps()[0]);
}

ImageSkia::~ImageSkia() {
}

void ImageSkia::AddBitmapForScale(const SkBitmap& bitmap,
                                  float dip_scale_factor) {
  DCHECK(!bitmap.isNull());

  if (isNull()) {
    Init(bitmap, dip_scale_factor);
  } else {
    // We currently assume that the bitmap size = 1x size * |dip_scale_factor|.
    // TODO(pkotwicz): Do something better because of rounding errors when
    // |dip_scale_factor| is not an int.
    // TODO(pkotwicz): Remove DCHECK for dip_scale_factor == 1.0f once
    // image_loading_tracker correctly handles multiple sized images.
    DCHECK(dip_scale_factor == 1.0f ||
              static_cast<int>(width() * dip_scale_factor) == bitmap.width());
    DCHECK(dip_scale_factor == 1.0f ||
              static_cast<int>(height() * dip_scale_factor) == bitmap.height());
    storage_->AddBitmap(bitmap);
  }
}

const SkBitmap& ImageSkia::GetBitmapForScale(float x_scale_factor,
                                             float y_scale_factor,
                                             float* bitmap_scale_factor) const {

  // Static null bitmap such that we are not returning a temporary.
  static SkBitmap* null_bitmap = new SkBitmap();

  if (empty())
    return *null_bitmap;

  // Get the desired bitmap width and height given |x_scale_factor|,
  // |y_scale_factor| and size at 1x density.
  float desired_width = width() * x_scale_factor;
  float desired_height = height() * y_scale_factor;

  const std::vector<SkBitmap>& bitmaps = storage_->bitmaps();
  size_t closest_index = 0;
  float smallest_diff = std::numeric_limits<float>::max();
  for (size_t i = 0; i < bitmaps.size(); ++i) {
    float diff = std::abs(bitmaps[i].width() - desired_width) +
        std::abs(bitmaps[i].height() - desired_height);
    if (diff < smallest_diff) {
      closest_index = i;
      smallest_diff = diff;
    }
  }
  if (smallest_diff < std::numeric_limits<float>::max()) {
    *bitmap_scale_factor = bitmaps[closest_index].width() / width();
    return bitmaps[closest_index];
  }

  return *null_bitmap;
}

bool ImageSkia::empty() const {
  return isNull() || storage_->size().IsEmpty();
}

int ImageSkia::width() const {
  return isNull() ? 0 : storage_->size().width();
}

int ImageSkia::height() const {
  return isNull() ? 0 : storage_->size().height();
}

bool ImageSkia::extractSubset(ImageSkia* dst, SkIRect& subset) const {
  if (isNull())
    return false;
  SkBitmap dst_bitmap;
  bool return_value  = storage_->bitmaps()[0].extractSubset(&dst_bitmap,
                                                            subset);
  *dst = ImageSkia(dst_bitmap);
  return return_value;
}

const std::vector<SkBitmap> ImageSkia::bitmaps() const {
  return storage_->bitmaps();
}

const SkBitmap* ImageSkia::bitmap() const {
  if (isNull())
    return NULL;

  return &storage_->bitmaps()[0];
}

void ImageSkia::Init(const SkBitmap& bitmap) {
  Init(bitmap, 1.0f);
}

void ImageSkia::Init(const SkBitmap& bitmap, float scale_factor) {
  DCHECK_GT(scale_factor, 0.0f);
  // TODO(pkotwicz): The image should be null whenever bitmap is null.
  if (bitmap.empty()) {
    storage_ = NULL;
    return;
  }
  storage_ = new internal::ImageSkiaStorage();
  storage_->set_size(gfx::Size(static_cast<int>(bitmap.width() / scale_factor),
      static_cast<int>(bitmap.height() / scale_factor)));
  storage_->AddBitmap(bitmap);
}

}  // namespace gfx
