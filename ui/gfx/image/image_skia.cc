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

  std::vector<gfx::ImageSkiaRep>& image_reps() { return image_reps_; }

  void set_size(const gfx::Size& size) { size_ = size; }
  const gfx::Size& size() const { return size_; }

 private:
  ~ImageSkiaStorage() {
  }

  // Vector of bitmaps and their associated scale factor.
  std::vector<gfx::ImageSkiaRep> image_reps_;

  // Size of the image in DIP.
  gfx::Size size_;

  friend class base::RefCounted<ImageSkiaStorage>;
};

}  // internal

ImageSkia::ImageSkia() : storage_(NULL) {
}

ImageSkia::ImageSkia(const SkBitmap& bitmap) {
  Init(ImageSkiaRep(bitmap));
}

ImageSkia::ImageSkia(const ImageSkiaRep& image_rep) {
  Init(image_rep);
}

ImageSkia::ImageSkia(const ImageSkia& other) : storage_(other.storage_) {
}

ImageSkia& ImageSkia::operator=(const ImageSkia& other) {
  storage_ = other.storage_;
  return *this;
}

ImageSkia& ImageSkia::operator=(const SkBitmap& other) {
  Init(ImageSkiaRep(other));
  return *this;
}

ImageSkia& ImageSkia::operator=(const ImageSkiaRep& other) {
  Init(other);
  return *this;
}

ImageSkia::operator SkBitmap&() const {
  if (isNull())
    return const_cast<SkBitmap&>(NullImageRep().sk_bitmap());

  return const_cast<SkBitmap&>(storage_->image_reps()[0].sk_bitmap());
}

ImageSkia::operator ImageSkiaRep&() const {
  if (isNull())
    return NullImageRep();

  return storage_->image_reps()[0];
}

ImageSkia::~ImageSkia() {
}

void ImageSkia::AddRepresentation(const ImageSkiaRep& image_rep) {
  DCHECK(!image_rep.is_null());

  if (isNull())
    Init(image_rep);
  else
    storage_->image_reps().push_back(image_rep);
}

void ImageSkia::RemoveRepresentation(ui::ScaleFactor scale_factor) {
  if (isNull())
    return;

  ImageSkiaReps& image_reps = storage_->image_reps();
  ImageSkiaReps::iterator it = FindRepresentation(scale_factor);
  if (it != image_reps.end() && it->scale_factor() == scale_factor)
    image_reps.erase(it);
}

bool ImageSkia::HasRepresentation(ui::ScaleFactor scale_factor) {
  if (isNull())
    return false;

  ImageSkiaReps::iterator it = FindRepresentation(scale_factor);
  return (it != storage_->image_reps().end() &&
          it->scale_factor() == scale_factor);
}

const ImageSkiaRep& ImageSkia::GetRepresentation(
    ui::ScaleFactor scale_factor) const {
  if (isNull())
    return NullImageRep();

  ImageSkiaReps::iterator it = FindRepresentation(scale_factor);
  if (it == storage_->image_reps().end())
    return NullImageRep();

  return *it;
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
  ImageSkia image;
  ImageSkiaReps& image_reps = storage_->image_reps();
  for (ImageSkiaReps::iterator it = image_reps.begin();
       it != image_reps.end(); ++it) {
    const ImageSkiaRep& image_rep = *it;
    float dip_scale = image_rep.GetScale();
    // Rounding boundary in case of a non-integer scale factor.
    int x = static_cast<int>(subset.left() * dip_scale + 0.5);
    int y = static_cast<int>(subset.top() * dip_scale + 0.5);
    int w = static_cast<int>(subset.width() * dip_scale + 0.5);
    int h = static_cast<int>(subset.height() * dip_scale + 0.5);
    SkBitmap dst_bitmap;
    SkIRect scaled_subset = SkIRect::MakeXYWH(x, y, w, h);
    if (image_rep.sk_bitmap().extractSubset(&dst_bitmap, scaled_subset))
      image.AddRepresentation(ImageSkiaRep(dst_bitmap,
                                           image_rep.scale_factor()));
  }
  if (image.empty())
    return false;

  *dst = image;
  return true;
}

std::vector<ImageSkiaRep> ImageSkia::image_reps() const {
  if (isNull())
    return std::vector<ImageSkiaRep>();

  return storage_->image_reps();
}

const SkBitmap* ImageSkia::bitmap() const {
  if (isNull()) {
    // Callers expect a ImageSkiaRep even if it is |isNull()|.
    // TODO(pkotwicz): Fix this.
    return &NullImageRep().sk_bitmap();
  }

  return &storage_->image_reps()[0].sk_bitmap();
}

void ImageSkia::Init(const ImageSkiaRep& image_rep) {
  // TODO(pkotwicz): The image should be null whenever image rep is null.
  if (image_rep.sk_bitmap().empty()) {
    storage_ = NULL;
    return;
  }
  storage_ = new internal::ImageSkiaStorage();
  storage_->set_size(gfx::Size(image_rep.GetWidth(), image_rep.GetHeight()));
  storage_->image_reps().push_back(image_rep);
}

// static
ImageSkiaRep& ImageSkia::NullImageRep() {
  CR_DEFINE_STATIC_LOCAL(ImageSkiaRep, null_image_rep, ());
  return null_image_rep;
}

std::vector<ImageSkiaRep>::iterator ImageSkia::FindRepresentation(
    ui::ScaleFactor scale_factor) const {
  DCHECK(!isNull());

  float scale = ui::GetScaleFactorScale(scale_factor);
  ImageSkiaReps& image_reps = storage_->image_reps();
  ImageSkiaReps::iterator closest_iter = image_reps.end();
  float smallest_diff = std::numeric_limits<float>::max();
  for (ImageSkiaReps::iterator it = image_reps.begin();
       it < image_reps.end();
       ++it) {
    float diff = std::abs(it->GetScale() - scale);
    if (diff < smallest_diff) {
      closest_iter = it;
      smallest_diff = diff;
    }
  }

  return closest_iter;
}

}  // namespace gfx
