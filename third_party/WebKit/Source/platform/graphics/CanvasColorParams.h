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
  // The default constructor will create an output-blended 8-bit surface.
  CanvasColorParams();
  CanvasColorParams(CanvasColorSpace, CanvasPixelFormat);
  explicit CanvasColorParams(const SkImageInfo&);
  CanvasColorSpace color_space() const { return color_space_; }
  CanvasPixelFormat pixel_format() const { return pixel_format_; }

  void SetCanvasColorSpace(CanvasColorSpace);
  void SetCanvasPixelFormat(CanvasPixelFormat);

  // Returns true if the canvas blends output color space values (that is,
  // not linear space colors).
  bool UsesOutputSpaceBlending() const;

  // Returns true if color correct rendering flag is set.
  static bool ColorCorrectRenderingEnabled();
  // Returns true if color correct rendering flag is set but canvas color
  // extensions flag is not.
  static bool ColorCorrectRenderingInSRGBOnly();
  // Returns true if both color correct rendering and canvas color extensions
  // flags are set. This activates the color management pipeline for all color
  // spaces.
  static bool ColorCorrectRenderingInAnyColorSpace();
  // Returns true if color correct rendering flag is set but color canvas
  // extensions flag is not set and the color space stored in CanvasColorParams
  // object is null.
  bool ColorCorrectNoColorSpaceToSRGB() const;

  // The SkColorSpace to use in the SkImageInfo for allocated SkSurfaces. This
  // is nullptr in legacy rendering mode.
  sk_sp<SkColorSpace> GetSkColorSpaceForSkSurfaces() const;

  // The pixel format to use for allocating SkSurfaces.
  SkColorType GetSkColorType() const;
  uint8_t BytesPerPixel() const;

  // The color space to use for compositing. This will always return a valid
  // gfx or skia color space.
  gfx::ColorSpace GetGfxColorSpace() const;
  sk_sp<SkColorSpace> GetSkColorSpace() const;

  // This matches CanvasRenderingContext::LinearPixelMath, and is true only when
  // the pixel format is half-float linear.
  // TODO(ccameron): This is not the same as !UsesOutputSpaceBlending, but
  // perhaps should be.
  bool LinearPixelMath() const;

 private:
  CanvasColorSpace color_space_ = kLegacyCanvasColorSpace;
  CanvasPixelFormat pixel_format_ = kRGBA8CanvasPixelFormat;
};

}  // namespace blink

#endif  // CanvasColorParams_h
