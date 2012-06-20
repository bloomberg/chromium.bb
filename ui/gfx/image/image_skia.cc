// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia.h"

#include <limits>
#include <cmath>

#include "base/logging.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"

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

  void RemoveBitmap(int position) {
    DCHECK_GE(position, 0);
    DCHECK_LT(static_cast<size_t>(position), bitmaps_.size());
    bitmaps_.erase(bitmaps_.begin() + position);
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

// static
SkBitmap* ImageSkia::null_bitmap_ = new SkBitmap();

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
    return *null_bitmap_;
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
    // TODO(pkotwicz): Figure out a way of dealing with adding HDA bitmaps. In
    // HDA, width() * |dip_scale_factor| != |bitmap|.width().
    storage_->AddBitmap(bitmap);
  }
}

void ImageSkia::RemoveBitmapForScale(float dip_scale_factor) {
  float bitmap_scale_factor;
  int remove_candidate = GetBitmapIndexForScale(dip_scale_factor,
                                                dip_scale_factor,
                                                &bitmap_scale_factor);
  // TODO(pkotwicz): Allow for small errors between scale factors due to
  // rounding errors in computing |bitmap_scale_factor|.
  if (remove_candidate >= 0 && dip_scale_factor == bitmap_scale_factor)
    storage_->RemoveBitmap(remove_candidate);
}

bool ImageSkia::HasBitmapForScale(float dip_scale_factor) {
  float bitmap_scale_factor;
  int candidate = GetBitmapIndexForScale(dip_scale_factor,
                                         dip_scale_factor,
                                         &bitmap_scale_factor);
  // TODO(pkotwicz): Allow for small errors between scale factors due to
  // rounding errors in computing |bitmap_scale_factor|.
  return (candidate >= 0 && bitmap_scale_factor == dip_scale_factor);
}

const SkBitmap& ImageSkia::GetBitmapForScale(float scale_factor,
                                             float* bitmap_scale_factor) const {
  return ImageSkia::GetBitmapForScale(scale_factor,
                                      scale_factor,
                                      bitmap_scale_factor);
}

const SkBitmap& ImageSkia::GetBitmapForScale(float x_scale_factor,
                                             float y_scale_factor,
                                             float* bitmap_scale_factor) const {
  int closest_index = GetBitmapIndexForScale(x_scale_factor, y_scale_factor,
      bitmap_scale_factor);

  if (closest_index < 0)
    return *null_bitmap_;

  const std::vector<SkBitmap>& bitmaps = storage_->bitmaps();
  return bitmaps[closest_index];
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

bool ImageSkia::extractSubset(ImageSkia* dst, const SkIRect& subset) const {
  if (isNull())
    return false;
  gfx::ImageSkia image;
  int dip_width = width();
  const std::vector<SkBitmap>& bitmaps = storage_->bitmaps();
  for (std::vector<SkBitmap>::const_iterator it = bitmaps.begin();
       it != bitmaps.end(); ++it) {
    const SkBitmap& bitmap = *it;
    int px_width = it->width();
    float dip_scale = static_cast<float>(px_width) /
        static_cast<float>(dip_width);
    // Rounding boundary in case of a non-integer scale factor.
    int x = static_cast<int>(subset.left() * dip_scale + 0.5);
    int y = static_cast<int>(subset.top() * dip_scale + 0.5);
    int w = static_cast<int>(subset.width() * dip_scale + 0.5);
    int h = static_cast<int>(subset.height() * dip_scale + 0.5);
    SkBitmap dst_bitmap;
    SkIRect scaled_subset = SkIRect::MakeXYWH(x, y, w, h);
    if (bitmap.extractSubset(&dst_bitmap, scaled_subset))
      image.AddBitmapForScale(dst_bitmap, dip_scale);
  }
  if (image.empty())
    return false;

  *dst = image;
  return true;
}

const std::vector<SkBitmap> ImageSkia::bitmaps() const {
  return storage_->bitmaps();
}

const SkBitmap* ImageSkia::bitmap() const {
  if (isNull()) {
    // Callers expect an SkBitmap even if it is |isNull()|.
    // TODO(pkotwicz): Fix this.
    return null_bitmap_;
  }

  return &storage_->bitmaps()[0];
}

void ImageSkia::Init(const SkBitmap& bitmap) {
  Init(bitmap, 1.0f);
}

void ImageSkia::Init(const SkBitmap& bitmap, float scale_factor) {
  DCHECK_GT(scale_factor, 0.0f);
  // TODO(pkotwicz): The image should be null whenever bitmap is null.
  if (bitmap.empty() && bitmap.isNull()) {
    storage_ = NULL;
    return;
  }
  storage_ = new internal::ImageSkiaStorage();
  storage_->set_size(gfx::Size(static_cast<int>(bitmap.width() / scale_factor),
      static_cast<int>(bitmap.height() / scale_factor)));
  storage_->AddBitmap(bitmap);
}

int ImageSkia::GetBitmapIndexForScale(float x_scale_factor,
                                      float y_scale_factor,
                                      float* bitmap_scale_factor) const {

  if (empty())
    return -1;

  // Get the desired bitmap width and height given |x_scale_factor|,
  // |y_scale_factor| and size at 1x density.
  float desired_width = width() * x_scale_factor;
  float desired_height = height() * y_scale_factor;

  const std::vector<SkBitmap>& bitmaps = storage_->bitmaps();
  int closest_index = -1;
  float smallest_diff = std::numeric_limits<float>::max();
  for (size_t i = 0; i < bitmaps.size(); ++i) {
    float diff = std::abs(bitmaps[i].width() - desired_width) +
        std::abs(bitmaps[i].height() - desired_height);
    if (diff < smallest_diff) {
      closest_index = static_cast<int>(i);
      smallest_diff = diff;
    }
  }

  if (closest_index >= 0) {
    *bitmap_scale_factor = static_cast<float>(bitmaps[closest_index].width()) /
        width();
  }

  return closest_index;
}

}  // namespace gfx
