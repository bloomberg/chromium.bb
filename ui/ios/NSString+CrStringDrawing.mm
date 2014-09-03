// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/ios/NSString+CrStringDrawing.h"

#include "base/logging.h"

namespace {
// Returns the closest pixel-aligned value higher than |value|, taking the scale
// factor into account. At a scale of 1, equivalent to ceil().
// TODO(lliabraa): Move this method to a common util file (crbug.com/409823).
CGFloat alignValueToUpperPixel(CGFloat value) {
  CGFloat scale = [[UIScreen mainScreen] scale];
  return ceil(value * scale) / scale;
}
}  // namespace

@implementation NSString (CrStringDrawing)

- (CGSize)cr_pixelAlignedSizeWithFont:(UIFont*)font {
  DCHECK(font) << "|font| can not be nil; it is used as a NSDictionary value";
  NSDictionary* attributes = @{ NSFontAttributeName : font };
  CGSize size = [self sizeWithAttributes:attributes];
  return CGSizeMake(alignValueToUpperPixel(size.width),
                    alignValueToUpperPixel(size.height));
}

- (CGSize)cr_sizeWithFont:(UIFont*)font {
  if (!font)
    return CGSizeZero;
  NSDictionary* attributes = @{ NSFontAttributeName : font };
  CGSize size = [self sizeWithAttributes:attributes];
  return CGSizeMake(ceil(size.width), ceil(size.height));
}

@end
