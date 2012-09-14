// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/memory/scoped_nsobject.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_ios.h"

namespace gfx {
namespace internal {

void PNGFromUIImage(UIImage* uiimage, std::vector<unsigned char>* png) {
  NSData* data = UIImagePNGRepresentation(uiimage);

  if ([data length] == 0)
    return;

  png->resize([data length]);
  [data getBytes:&png->at(0) length:[data length]];
}

UIImage* CreateUIImageFromPNG(const std::vector<unsigned char>& png) {
  NSData* data = [NSData dataWithBytes:&png.front() length:png.size()];
  scoped_nsobject<UIImage> image ([[UIImage alloc] initWithData:data]);
  if (!image) {
    LOG(WARNING) << "Unable to decode PNG into UIImage.";
    // Return a 16x16 red image to visually show error.
    UIGraphicsBeginImageContext(CGSizeMake(16, 16));
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGContextSetRGBFillColor(context, 1.0, 0.0, 0.0, 1.0);
    CGContextFillRect(context, CGRectMake(0.0, 0.0, 16, 16));
    image.reset([UIGraphicsGetImageFromCurrentImageContext() retain]);
    UIGraphicsEndImageContext();
  }
  return image.release();
}

void PNGFromImageSkia(const ImageSkia* skia, std::vector<unsigned char>* png) {
  // iOS does not expose libpng, so conversion from ImageSkia to PNG must go
  // through UIImage.
  // TODO(rohitrao): Rewrite the callers of this function to save the UIImage
  // representation in the gfx::Image.  If we're generating it, we might as well
  // hold on to it.
  UIImage* image = UIImageFromImageSkia(*skia);
  PNGFromUIImage(image, png);
}

ImageSkia* ImageSkiaFromPNG(const std::vector<unsigned char>& png) {
  // iOS does not expose libpng, so conversion from PNG to ImageSkia must go
  // through UIImage.
  scoped_nsobject<UIImage> uiimage(CreateUIImageFromPNG(png));
  if (!uiimage) {
    LOG(WARNING) << "Unable to decode PNG into ImageSkia.";
    // Return a 16x16 red image to visually show error.
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
    bitmap.allocPixels();
    bitmap.eraseRGB(0xff, 0, 0);
    return new ImageSkia(bitmap);
  }

  return new ImageSkia(ImageSkiaFromUIImage(uiimage));
}

} // namespace internal
} // namespace gfx
