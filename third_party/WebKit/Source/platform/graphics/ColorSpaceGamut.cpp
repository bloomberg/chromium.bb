// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/ColorSpaceGamut.h"

#include "public/platform/WebScreenInfo.h"
#include "third_party/skia/include/core/SkColorSpaceXform.h"

namespace blink {

namespace ColorSpaceUtilities {

ColorSpaceGamut GetColorSpaceGamut(const WebScreenInfo& screen_info) {
  const gfx::ColorSpace& color_space = screen_info.color_space;
  if (!color_space.IsValid())
    return ColorSpaceGamut::kUnknown;

  return ColorSpaceUtilities::GetColorSpaceGamut(
      color_space.ToSkColorSpace().get());
}

ColorSpaceGamut GetColorSpaceGamut(SkColorSpace* color_space) {
  sk_sp<SkColorSpace> sc_rgb(SkColorSpace::MakeSRGBLinear());
  std::unique_ptr<SkColorSpaceXform> transform(
      SkColorSpaceXform::New(color_space, sc_rgb.get()));

  if (!transform)
    return ColorSpaceGamut::kUnknown;

  unsigned char in[3][4];
  float out[3][4];
  memset(in, 0, sizeof(in));
  in[0][0] = 255;
  in[1][1] = 255;
  in[2][2] = 255;
  in[0][3] = 255;
  in[1][3] = 255;
  in[2][3] = 255;
  transform->apply(SkColorSpaceXform::kRGBA_F32_ColorFormat, out,
                   SkColorSpaceXform::kRGBA_8888_ColorFormat, in, 3,
                   kOpaque_SkAlphaType);
  float score = out[0][0] * out[1][1] * out[2][2];

  if (score < 0.9)
    return ColorSpaceGamut::kLessThanNTSC;
  if (score < 0.95)
    return ColorSpaceGamut::NTSC;  // actual score 0.912839
  if (score < 1.1)
    return ColorSpaceGamut::SRGB;  // actual score 1.0
  if (score < 1.3)
    return ColorSpaceGamut::kAlmostP3;
  if (score < 1.425)
    return ColorSpaceGamut::P3;  // actual score 1.401899
  if (score < 1.5)
    return ColorSpaceGamut::kAdobeRGB;  // actual score 1.458385
  if (score < 2.0)
    return ColorSpaceGamut::kWide;
  if (score < 2.2)
    return ColorSpaceGamut::BT2020;  // actual score 2.104520
  if (score < 2.7)
    return ColorSpaceGamut::kProPhoto;  // actual score 2.913247
  return ColorSpaceGamut::kUltraWide;
}

}  // namespace ColorSpaceUtilities

}  // namespace blink
