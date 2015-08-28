// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_PIXEL_REF_UTILS_H_
#define SKIA_EXT_PIXEL_REF_UTILS_H_

#include <vector>

#include "SkPicture.h"
#include "SkRect.h"

namespace skia {

class SK_API DiscardableImageUtils {
 public:
  struct PositionImage {
    const SkImage* image;
    SkRect image_rect;
    SkMatrix matrix;
    SkFilterQuality filter_quality;
  };

  static void GatherDiscardableImages(SkPicture* picture,
                                      std::vector<PositionImage>* images);
};

using PositionImage = DiscardableImageUtils::PositionImage;
using DiscardableImageList = std::vector<PositionImage>;

}  // namespace skia

#endif
