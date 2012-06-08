// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>
#import <vector>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
namespace internal {

gfx::ImageSkia NSImageToImageSkia(NSImage* image) {
  gfx::ImageSkia image_skia;
  for (NSImageRep* imageRep in [image representations]) {
    NSSize imageRepSize = [imageRep size];
    SkBitmap bitmap(gfx::NSImageRepToSkBitmap(imageRep, imageRepSize, false));
    if (!bitmap.isNull() && !bitmap.empty()) {
      float scaleFactor = imageRepSize.width / [image size].width;
      image_skia.AddBitmapForScale(bitmap, scaleFactor);
    }
  }
  return image_skia;
}

NSImage* ImageSkiaToNSImage(const gfx::ImageSkia* image_skia) {
  if (image_skia->empty())
    return nil;

  scoped_nsobject<NSImage> image([[NSImage alloc] init]);

  const std::vector<SkBitmap> bitmaps = image_skia->bitmaps();
  for (std::vector<SkBitmap>::const_iterator it = bitmaps.begin();
       it != bitmaps.end(); ++it) {
    [image addRepresentation:gfx::SkBitmapToNSBitmapImageRep(*it)];
  }

  [image setSize:NSMakeSize(image_skia->width(), image_skia->height())];
  return [image.release() autorelease];
}

}  // namespace internal
}  // namespace gfx
