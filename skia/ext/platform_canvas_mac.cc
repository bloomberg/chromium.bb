// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/platform_canvas.h"

#include "skia/ext/bitmap_platform_device_mac.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace skia {

PlatformCanvas::PlatformCanvas(int width, int height, bool is_opaque)
    : SkCanvas(SkNEW(BitmapPlatformDeviceFactory)) {
  initialize(width, height, is_opaque);
}

PlatformCanvas::PlatformCanvas(int width,
                               int height,
                               bool is_opaque,
                               CGContextRef context)
    : SkCanvas(SkNEW(BitmapPlatformDeviceFactory)) {
  initialize(width, height, is_opaque);
}

PlatformCanvas::PlatformCanvas(int width,
                               int height,
                               bool is_opaque,
                               uint8_t* data)
    : SkCanvas(SkNEW(BitmapPlatformDeviceFactory)) {
  initialize(width, height, is_opaque, data);
}

PlatformCanvas::~PlatformCanvas() {
}

bool PlatformCanvas::initialize(int width,
                                int height,
                                bool is_opaque,
                                uint8_t* data) {
  SkDevice* device = BitmapPlatformDevice::CreateWithData(data, width, height,
                                                          is_opaque);
  if (!device)
    return false;

  setDevice(device);
  device->unref();  // Was created with refcount 1, and setDevice also refs.
  return true;
}

CGContextRef PlatformCanvas::beginPlatformPaint() {
  return getTopPlatformDevice().GetBitmapContext();
}

void PlatformCanvas::endPlatformPaint() {
  // Flushing will be done in onAccessBitmap.
}

}  // namespace skia
