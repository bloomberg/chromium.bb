// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "base/memory/scoped_ptr.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {
namespace internal {

bool NSImageToSkBitmaps(NSImage* image, std::vector<const SkBitmap*>* bitmaps) {
  for (NSImageRep* imageRep in [image representations]) {
    scoped_ptr<SkBitmap> bitmap(new SkBitmap(
        gfx::NSImageRepToSkBitmap(imageRep, [imageRep size], false)));
    if (bitmap->isNull())
      return false;
    bitmaps->push_back(bitmap.release());
  }
  return true;
}

}  // namespace internal
}  // namespace gfx
