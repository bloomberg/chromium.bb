// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/platform_device.h"
#include "skia/ext/bitmap_platform_device.h"

#import <ApplicationServices/ApplicationServices.h>
#include "skia/ext/skia_utils_mac.h"

namespace skia {

CGContextRef PlatformDevice::BeginPlatformPaint() {
  return GetBitmapContext();
}

void PlatformDevice::EndPlatformPaint() {
  // Flushing will be done in onAccessBitmap.
}

}  // namespace skia
