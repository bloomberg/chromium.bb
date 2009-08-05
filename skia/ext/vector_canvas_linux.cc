// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/vector_canvas.h"

#include "skia/ext/bitmap_platform_device.h"
#include "skia/ext/vector_platform_device.h"

namespace skia {

VectorCanvas::VectorCanvas(int width, int height) {
  bool initialized = initialize(width, height);

  SkASSERT(initialized);
}

bool VectorCanvas::initialize(int width, int height) {
  SkDevice* device = createPlatformDevice(width, height, true);
  if (!device)
    return false;

  setDevice(device);
  device->unref();  // was created with refcount 1, and setDevice also refs
  return true;
}

SkDevice* VectorCanvas::createDevice(SkBitmap::Config config,
                                     int width, int height,
                                     bool is_opaque, bool isForLayer) {
  SkASSERT(config == SkBitmap::kARGB_8888_Config);
  return createPlatformDevice(width, height, is_opaque);
}

SkDevice* VectorCanvas::createPlatformDevice(int width,
                                             int height, bool is_opaque) {
  // TODO(myhuang): Here we might also have similar issues as those on Windows
  // (vector_canvas_win.cc, http://crbug.com/18382 & http://crbug.com/18383).
  // Please note that is_opaque is true when we use this class for printing.
  if (!is_opaque) {
    return BitmapPlatformDevice::Create(width, height, is_opaque);
  }

  PlatformDevice* device = VectorPlatformDevice::create(width, height);
  return device;
}

}  // namespace skia

