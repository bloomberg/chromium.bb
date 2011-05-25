// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/platform_canvas.h"

#include "skia/ext/bitmap_platform_device_mac.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace skia {

PlatformCanvas::PlatformCanvas(int width, int height, bool is_opaque) {
  setDeviceFactory(SkNEW(BitmapPlatformDeviceFactory))->unref();
  initialize(width, height, is_opaque);
}

PlatformCanvas::PlatformCanvas(int width,
                               int height,
                               bool is_opaque,
                               CGContextRef context) {
  setDeviceFactory(SkNEW(BitmapPlatformDeviceFactory))->unref();
  initialize(context, width, height, is_opaque);
}

PlatformCanvas::PlatformCanvas(int width,
                               int height,
                               bool is_opaque,
                               uint8_t* data) {
  setDeviceFactory(SkNEW(BitmapPlatformDeviceFactory))->unref();
  initialize(width, height, is_opaque, data);
}

PlatformCanvas::~PlatformCanvas() {
}

bool PlatformCanvas::initialize(int width,
                                int height,
                                bool is_opaque,
                                uint8_t* data) {
  return initializeWithDevice(BitmapPlatformDevice::CreateWithData(
      data, width, height, is_opaque));
}

bool PlatformCanvas::initialize(CGContextRef context,
                                int width,
                                int height,
                                bool is_opaque) {
  return initializeWithDevice(BitmapPlatformDevice::Create(
      context, width, height, is_opaque));
}

CGContextRef PlatformCanvas::beginPlatformPaint() const {
  return getTopPlatformDevice().BeginPlatformPaint();
}

void PlatformCanvas::endPlatformPaint() const {
  getTopPlatformDevice().EndPlatformPaint();
}

}  // namespace skia
