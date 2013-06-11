// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_LAZY_PIXEL_REF_UTILS_H_
#define SKIA_EXT_LAZY_PIXEL_REF_UTILS_H_

#include <vector>

#include "SkPicture.h"
#include "SkRect.h"

namespace skia {

class LazyPixelRef;
class SK_API LazyPixelRefUtils {
 public:

  struct PositionLazyPixelRef {
    skia::LazyPixelRef* lazy_pixel_ref;
    SkRect pixel_ref_rect;
  };

  static void GatherPixelRefs(
      SkPicture* picture,
      std::vector<PositionLazyPixelRef>* lazy_pixel_refs);
};

typedef std::vector<LazyPixelRefUtils::PositionLazyPixelRef> LazyPixelRefList;

}  // namespace skia

#endif
