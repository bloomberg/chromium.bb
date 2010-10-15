// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/vector_canvas.h"

#include "skia/ext/bitmap_platform_device.h"
#include "skia/ext/vector_platform_device.h"

namespace skia {

VectorCanvas::VectorCanvas(HDC dc, int width, int height) {
  bool initialized = initialize(dc, width, height);
  if (!initialized)
    __debugbreak();
}

bool VectorCanvas::initialize(HDC context, int width, int height) {
  SkDevice* device = SkVectorPlatformDeviceFactory::CreateDevice(width, height,
                                                                 true, context);
  if (!device)
    return false;

  setDevice(device);
  device->unref();  // was created with refcount 1, and setDevice also refs
  return true;
}

}  // namespace skia

