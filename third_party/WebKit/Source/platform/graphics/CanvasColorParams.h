// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasColorParams_h
#define CanvasColorParams_h

#include "platform/PlatformExport.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace gfx {
class ColorSpace;
}

namespace blink {

enum CanvasColorSpace {
  kLegacyCanvasColorSpace,
  kSRGBCanvasColorSpace,
  kRec2020CanvasColorSpace,
  kP3CanvasColorSpace,
};

enum CanvasPixelFormat {
  kRGBA8CanvasPixelFormat,
  kRGB10A2CanvasPixelFormat,
  kRGBA12CanvasPixelFormat,
  kF16CanvasPixelFormat,
};

class PLATFORM_EXPORT CanvasColorParams {
 public:
  CanvasColorParams(CanvasColorSpace, CanvasPixelFormat);
  CanvasColorSpace color_space() const { return color_space_; }
  CanvasPixelFormat pixel_format() const { return pixel_format_; }

  // The SkColorSpace to use in the SkImageInfo for allocated SkSurfaces. This
  // is nullptr in legacy rendering mode.
  sk_sp<SkColorSpace> GetSkColorSpaceForSkSurfaces() const;

  // The pixel format to use for allocating SkSurfaces.
  SkColorType GetSkColorType() const;

  // The color space to use for compositing.
  gfx::ColorSpace GetGfxColorSpace() const;

  // This matches CanvasRenderingContext::LinearPixelMath, and is true only when
  // the pixel format is half-float linear.
  bool LinearPixelMath() const;

 private:
  CanvasColorSpace color_space_ = kLegacyCanvasColorSpace;
  CanvasPixelFormat pixel_format_ = kRGBA8CanvasPixelFormat;
};

}  // namespace blink

#endif  // CanvasColorParams_h
