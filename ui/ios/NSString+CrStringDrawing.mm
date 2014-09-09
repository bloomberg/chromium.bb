// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/ios/NSString+CrStringDrawing.h"

#include "base/logging.h"
#include "ui/ios/uikit_util.h"

@implementation NSString (CrStringDrawing)

- (CGSize)cr_pixelAlignedSizeWithFont:(UIFont*)font {
  DCHECK(font) << "|font| can not be nil; it is used as a NSDictionary value";
  NSDictionary* attributes = @{ NSFontAttributeName : font };
  return ui::AlignSizeToUpperPixel([self sizeWithAttributes:attributes]);
}

- (CGSize)cr_sizeWithFont:(UIFont*)font {
  if (!font)
    return CGSizeZero;
  NSDictionary* attributes = @{ NSFontAttributeName : font };
  CGSize size = [self sizeWithAttributes:attributes];
  return CGSizeMake(ceil(size.width), ceil(size.height));
}

@end
