// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_VECTOR_PLATFORM_DEVICE_SKIA_H_
#define SKIA_EXT_VECTOR_PLATFORM_DEVICE_SKIA_H_
#pragma once

#include "base/basictypes.h"
#include "base/logging.h"
#include "skia/ext/platform_device.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTScopedPtr.h"
#include "third_party/skia/include/pdf/SkPDFDevice.h"

class SkClipStack;
class SkMatrix;
struct SkIRect;
struct SkRect;

namespace skia {

class BitmapPlatformDevice;

class VectorPlatformDeviceSkia : public PlatformDevice, public SkPDFDevice {
 public:
  SK_API VectorPlatformDeviceSkia(const SkISize& pageSize,
                                  const SkISize& contentSize,
                                  const SkMatrix& initialTransform);
  virtual ~VectorPlatformDeviceSkia();

  // PlatformDevice methods.
  virtual bool IsNativeFontRenderingAllowed();

  virtual PlatformSurface BeginPlatformPaint();
  virtual void EndPlatformPaint();
#if defined(OS_WIN)
  virtual void DrawToNativeContext(HDC dc, int x, int y, const RECT* src_rect);
#elif defined(OS_MACOSX)
  virtual void DrawToNativeContext(CGContext* context, int x, int y,
                                   const CGRect* src_rect);
  virtual CGContextRef GetBitmapContext();
#elif defined(OS_LINUX)
  virtual void DrawToNativeContext(PlatformSurface surface, int x, int y,
                                   const PlatformRect* src_rect);
#endif

 private:
  SkRefPtr<BitmapPlatformDevice> raster_surface_;

  DISALLOW_COPY_AND_ASSIGN(VectorPlatformDeviceSkia);
};

}  // namespace skia

#endif  // SKIA_EXT_VECTOR_PLATFORM_DEVICE_SKIA_H_
