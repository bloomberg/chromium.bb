// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia_util_mac.h"

#import <AppKit/AppKit.h>

#include "base/mac/mac_util.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {

gfx::ImageSkia ImageSkiaFromNSImage(NSImage* image) {
  return ImageSkiaFromResizedNSImage(image, [image size]);
}

gfx::ImageSkia ImageSkiaFromResizedNSImage(NSImage* image,
                                           NSSize desired_size) {
  // Resize and convert to ImageSkia simultaneously to save on computation.
  // TODO(pkotwicz): Separate resizing NSImage and converting to ImageSkia.
  float resize_scale_x = desired_size.width / [image size].width;
  float resize_scale_y = desired_size.height / [image size].height;

  gfx::ImageSkia image_skia;
  for (NSImageRep* image_rep in [image representations]) {
    NSSize image_rep_size = NSMakeSize([image_rep pixelsWide] * resize_scale_x,
        [image_rep pixelsHigh] * resize_scale_y);
    SkBitmap bitmap(gfx::NSImageRepToSkBitmap(image_rep, image_rep_size,
        false));
    if (!bitmap.isNull() && !bitmap.empty()) {
      float scale_factor = image_rep_size.width / desired_size.width;
      image_skia.AddBitmapForScale(bitmap, scale_factor);
    }
  }
  return image_skia;
}

gfx::ImageSkia ApplicationIconAtSize(int desired_size) {
  NSImage* image = [NSImage imageNamed:@"NSApplicationIcon"];
  return ImageSkiaFromResizedNSImage(image,
                                     NSMakeSize(desired_size, desired_size));
}

NSImage* NSImageFromImageSkia(const gfx::ImageSkia& image_skia) {
  if (image_skia.empty())
    return nil;

  scoped_nsobject<NSImage> image([[NSImage alloc] init]);

  const std::vector<SkBitmap> bitmaps = image_skia.bitmaps();
  for (std::vector<SkBitmap>::const_iterator it = bitmaps.begin();
       it != bitmaps.end(); ++it) {
    [image addRepresentation:gfx::SkBitmapToNSBitmapImageRep(*it)];
  }

  [image setSize:NSMakeSize(image_skia.width(), image_skia.height())];
  return [image.release() autorelease];
}

}  // namespace gfx
