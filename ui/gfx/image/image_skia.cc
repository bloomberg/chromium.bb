// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"

namespace gfx {
namespace {

// static
gfx::ImageSkiaRep& NullImageRep() {
  CR_DEFINE_STATIC_LOCAL(ImageSkiaRep, null_image_rep, ());
  return null_image_rep;
}

}  // namespace

namespace internal {
namespace {

class Matcher {
 public:
  explicit Matcher(ui::ScaleFactor scale_factor) : scale_factor_(scale_factor) {
  }

  bool operator()(const ImageSkiaRep& rep) const {
    return rep.scale_factor() == scale_factor_;
  }

 private:
  ui::ScaleFactor scale_factor_;
};

}  // namespace

// A helper class such that ImageSkia can be cheaply copied. ImageSkia holds a
// refptr instance of ImageSkiaStorage, which in turn holds all of ImageSkia's
// information.
class ImageSkiaStorage : public base::RefCounted<ImageSkiaStorage> {
 public:
  ImageSkiaStorage(ImageSkiaSource* source, const gfx::Size& size)
      : source_(source),
        size_(size) {
  }

  std::vector<gfx::ImageSkiaRep>& image_reps() { return image_reps_; }

  const gfx::Size& size() const { return size_; }

  // Returns the iterator of the image rep whose density best matches
  // |scale_factor|. If the image for the |scale_factor| doesn't exist
  // in the storage and |storage| is set, it fetches new image by calling
  // |ImageSkiaSource::GetImageForScale|. If the source returns the
  // image with different scale factor (if the image doesn't exist in
  // resource, for example), it will fallback to closest image rep.
  std::vector<ImageSkiaRep>::iterator FindRepresentation(
      ui::ScaleFactor scale_factor, bool fetch_new_image) const {
    ImageSkiaStorage* non_const = const_cast<ImageSkiaStorage*>(this);

    float scale = ui::GetScaleFactorScale(scale_factor);
    ImageSkia::ImageSkiaReps::iterator closest_iter =
        non_const->image_reps().end();
    ImageSkia::ImageSkiaReps::iterator exact_iter =
        non_const->image_reps().end();
    float smallest_diff = std::numeric_limits<float>::max();
    for (ImageSkia::ImageSkiaReps::iterator it =
             non_const->image_reps().begin();
         it < image_reps_.end(); ++it) {
      if (it->GetScale() == scale) {
        // found exact match
        fetch_new_image = false;
        if (it->is_null())
          continue;
        exact_iter = it;
        break;
      }
      float diff = std::abs(it->GetScale() - scale);
      if (diff < smallest_diff && !it->is_null()) {
        closest_iter = it;
        smallest_diff = diff;
      }
    }

    if (fetch_new_image && source_.get()) {
      ImageSkiaRep image = source_->GetImageForScale(scale_factor);

      // If the source returned the new image, store it.
      if (!image.is_null() &&
          std::find_if(image_reps_.begin(), image_reps_.end(),
                       Matcher(image.scale_factor())) == image_reps_.end()) {
        non_const->image_reps().push_back(image);
      }

      // If the result image's scale factor isn't same as the expected
      // scale factor, create null ImageSkiaRep with the |scale_factor|
      // so that the next lookup will fallback to the closest scale.
      if (image.is_null() || image.scale_factor() != scale_factor) {
        non_const->image_reps().push_back(
            ImageSkiaRep(SkBitmap(), scale_factor));
      }

      // image_reps_ must have the exact much now, so find again.
      return FindRepresentation(scale_factor, false);
    }
    return exact_iter != image_reps_.end() ? exact_iter : closest_iter;
  }

 private:
  ~ImageSkiaStorage() {
  }

  // Vector of bitmaps and their associated scale factor.
  std::vector<gfx::ImageSkiaRep> image_reps_;

  scoped_ptr<ImageSkiaSource> source_;

  // Size of the image in DIP.
  const gfx::Size size_;

  friend class base::RefCounted<ImageSkiaStorage>;
};

}  // internal

ImageSkia::ImageSkia() : storage_(NULL) {
}

ImageSkia::ImageSkia(ImageSkiaSource* source, const gfx::Size& size)
    : storage_(new internal::ImageSkiaStorage(source, size)) {
  DCHECK(source);
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
  ImageSkiaReps::iterator it =
      storage_->FindRepresentation(scale_factor, false);
  if (it != image_reps.end() && it->scale_factor() == scale_factor)
    image_reps.erase(it);
}

bool ImageSkia::HasRepresentation(ui::ScaleFactor scale_factor) {
  if (isNull())
    return false;

  ImageSkiaReps::iterator it =
      storage_->FindRepresentation(scale_factor, false);
  return (it != storage_->image_reps().end() &&
          it->scale_factor() == scale_factor);
}

const ImageSkiaRep& ImageSkia::GetRepresentation(
    ui::ScaleFactor scale_factor) const {
  if (isNull())
    return NullImageRep();

  ImageSkiaReps::iterator it = storage_->FindRepresentation(scale_factor, true);
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

gfx::Size ImageSkia::size() const {
  return gfx::Size(width(), height());
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
  storage_ = new internal::ImageSkiaStorage(
      NULL, gfx::Size(image_rep.GetWidth(), image_rep.GetHeight()));
  storage_->image_reps().push_back(image_rep);
}

}  // namespace gfx
