// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_VECTOR_PLATFORM_DEVICE_SKIA_H_
#define SKIA_EXT_VECTOR_PLATFORM_DEVICE_SKIA_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "skia/ext/platform_device.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/pdf/SkPDFDevice.h"

class SkMatrix;

namespace skia {

class BitmapPlatformDevice;

class VectorPlatformDeviceSkia : public SkPDFDevice, public PlatformDevice {
 public:
  SK_API VectorPlatformDeviceSkia(const SkISize& pageSize,
                                  const SkISize& contentSize,
                                  const SkMatrix& initialTransform);
  ~VectorPlatformDeviceSkia() override;

  // PlatformDevice methods.
  bool SupportsPlatformPaint() override;

  PlatformSurface BeginPlatformPaint() override;
  void EndPlatformPaint() override;
#if defined(OS_WIN)
  virtual void DrawToNativeContext(HDC dc,
                                   int x,
                                   int y,
                                   const RECT* src_rect) override;
#elif defined(OS_MACOSX)
  void DrawToNativeContext(CGContext* context,
                           int x,
                           int y,
                           const CGRect* src_rect) override;
  CGContextRef GetBitmapContext() override;
#elif defined(OS_POSIX)
  virtual void DrawToNativeContext(PlatformSurface surface,
                                   int x,
                                   int y,
                                   const PlatformRect* src_rect) override;
#endif

 private:
  skia::RefPtr<BitmapPlatformDevice> raster_surface_;

  DISALLOW_COPY_AND_ASSIGN(VectorPlatformDeviceSkia);
};

}  // namespace skia

#endif  // SKIA_EXT_VECTOR_PLATFORM_DEVICE_SKIA_H_
