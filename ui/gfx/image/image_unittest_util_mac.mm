// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "skia/ext/skia_utils_mac.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace gfx {
namespace test {

SkColor GetPlatformImageColor(PlatformImage image) {
  [image lockFocus];
  NSColor* color = NSReadPixel(NSMakePoint(10, 10));
  [image unlockFocus];
  return NSDeviceColorToSkColor(color);
}

}  // namespace test
}  // namespace gfx
