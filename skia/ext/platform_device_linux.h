// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_PLATFORM_DEVICE_LINUX_H_
#define SKIA_EXT_PLATFORM_DEVICE_LINUX_H_
#pragma once

#include "skia/ext/platform_device.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace skia {

// Blindly copying the mac hierarchy.
class PlatformDevice : public SkDevice {
 public:
  typedef cairo_t* PlatformSurface;

  // Returns if native platform APIs are allowed to render text to this device.
  virtual bool IsNativeFontRenderingAllowed();

  virtual PlatformSurface BeginPlatformPaint() = 0;
  virtual void EndPlatformPaint();

  virtual void DrawToNativeContext(PlatformSurface surface, int x, int y,
                                   const PlatformRect* src_rect );
                                   
  // Sets the opacity of each pixel in the specified region to be opaque.
  virtual void MakeOpaque(int x, int y, int width, int height) { }

 protected:
  // Forwards |bitmap| to SkDevice's constructor.
  explicit PlatformDevice(const SkBitmap& bitmap);
};

}  // namespace skia

#endif  // SKIA_EXT_PLATFORM_DEVICE_LINUX_H_
