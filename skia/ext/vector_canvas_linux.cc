// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/vector_canvas.h"

#include "skia/ext/vector_platform_device.h"

namespace skia {

VectorCanvas::VectorCanvas(cairo_t* context, int width, int height) {
  bool initialized = initialize(context, width, height);

  SkASSERT(initialized);
}

bool VectorCanvas::initialize(cairo_t* context, int width, int height) {
  SkDevice* device = VectorPlatformDeviceFactory::CreateDevice(context, width,
                                                               height, true);
  if (!device)
    return false;

  setDevice(device);
  device->unref();  // was created with refcount 1, and setDevice also refs
  return true;
}

}  // namespace skia
