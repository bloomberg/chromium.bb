// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "third_party/google_toolbox_for_mac/src/iPhone/GTMUIImage+Resize.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"
#include "ui/gfx/image/resize_image_dimensions.h"

#include "base/logging.h"

namespace gfx {

bool JPEG1xEncodedDataFromImage(const Image& image,
                                int quality,
                                std::vector<unsigned char>* dst) {
  NSData* data = UIImageJPEGRepresentation(image.ToUIImage(), quality / 100.0);

  if ([data length] == 0)
    return false;

  dst->resize([data length]);
  [data getBytes:&dst->at(0) length:[data length]];
  return true;
}

Image ResizedImageForSearchByImage(const Image& image) {
  if (image.IsEmpty()) {
    return image;
  }
  UIImage* ui_image = image.ToUIImage();

  if (ui_image &&
      ui_image.size.height * ui_image.size.width > kSearchByImageMaxImageArea &&
      (ui_image.size.width > kSearchByImageMaxImageWidth ||
       ui_image.size.height > kSearchByImageMaxImageHeight)) {
    CGSize new_image_size =
        CGSizeMake(kSearchByImageMaxImageWidth, kSearchByImageMaxImageHeight);
    ui_image = [ui_image gtm_imageByResizingToSize:new_image_size
                               preserveAspectRatio:YES
                                         trimToFit:NO];
  }
  return Image(ui_image);
}

}  // end namespace gfx
