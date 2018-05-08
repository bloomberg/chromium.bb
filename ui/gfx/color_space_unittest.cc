// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <tuple>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/skia_color_space_util.h"

namespace gfx {
namespace {

const float kEpsilon = 1.0e-3f;

// Returns the L-infty difference of u and v.
float Diff(const SkVector4& u, const SkVector4& v) {
  float result = 0;
  for (size_t i = 0; i < 4; ++i)
    result = std::max(result, std::abs(u.fData[i] - v.fData[i]));
  return result;
}

TEST(ColorSpace, RGBToYUV) {
  const size_t kNumTestRGBs = 3;
  SkVector4 test_rgbs[kNumTestRGBs] = {
      SkVector4(1.f, 0.f, 0.f, 1.f),
      SkVector4(0.f, 1.f, 0.f, 1.f),
      SkVector4(0.f, 0.f, 1.f, 1.f),
  };

  const size_t kNumColorSpaces = 4;
  gfx::ColorSpace color_spaces[kNumColorSpaces] = {
      gfx::ColorSpace::CreateREC601(), gfx::ColorSpace::CreateREC709(),
      gfx::ColorSpace::CreateJpeg(), gfx::ColorSpace::CreateXYZD50(),
  };

  SkVector4 expected_yuvs[kNumColorSpaces][kNumTestRGBs] = {
      // REC601
      {
          SkVector4(0.3195f, 0.3518f, 0.9392f, 1.0000f),
          SkVector4(0.5669f, 0.2090f, 0.1322f, 1.0000f),
          SkVector4(0.1607f, 0.9392f, 0.4286f, 1.0000f),
      },
      // REC709
      {
          SkVector4(0.2453f, 0.3994f, 0.9392f, 1.0000f),
          SkVector4(0.6770f, 0.1614f, 0.1011f, 1.0000f),
          SkVector4(0.1248f, 0.9392f, 0.4597f, 1.0000f),
      },
      // Jpeg
      {
          SkVector4(0.2990f, 0.3313f, 1.0000f, 1.0000f),
          SkVector4(0.5870f, 0.1687f, 0.0813f, 1.0000f),
          SkVector4(0.1140f, 1.0000f, 0.4187f, 1.0000f),
      },
      // XYZD50
      {
          SkVector4(1.0000f, 0.0000f, 0.0000f, 1.0000f),
          SkVector4(0.0000f, 1.0000f, 0.0000f, 1.0000f),
          SkVector4(0.0000f, 0.0000f, 1.0000f, 1.0000f),
      },
  };

  for (size_t i = 0; i < kNumColorSpaces; ++i) {
    SkMatrix44 transfer;
    color_spaces[i].GetTransferMatrix(&transfer);

    SkMatrix44 range_adjust;
    color_spaces[i].GetRangeAdjustMatrix(&range_adjust);

    SkMatrix44 range_adjust_inv;
    range_adjust.invert(&range_adjust_inv);

    for (size_t j = 0; j < kNumTestRGBs; ++j) {
      SkVector4 yuv = range_adjust_inv * transfer * test_rgbs[j];
      EXPECT_LT(Diff(yuv, expected_yuvs[i][j]), kEpsilon);
    }
  }
}

TEST(ColorSpace, RasterAndBlend) {
  ColorSpace display_color_space;

  // A linear transfer function being used for HDR should be blended using an
  // sRGB-like transfer function.
  display_color_space = ColorSpace::CreateSCRGBLinear();
  EXPECT_EQ(ColorSpace::CreateExtendedSRGB(),
            display_color_space.GetBlendingColorSpace());
  EXPECT_EQ(ColorSpace::CreateDisplayP3D65(),
            display_color_space.GetRasterColorSpace());

  // If not used for HDR, a linear transfer function should be left unchanged.
  display_color_space = ColorSpace::CreateXYZD50();
  EXPECT_EQ(display_color_space, display_color_space.GetBlendingColorSpace());
  EXPECT_EQ(display_color_space, display_color_space.GetRasterColorSpace());
}

TEST(ColorSpace, ToSkColorSpace) {
  const size_t kNumTests = 5;
  SkMatrix44 primary_matrix;
  primary_matrix.set3x3(0.205276f, 0.149185f, 0.609741f, 0.625671f, 0.063217f,
                        0.311111f, 0.060867f, 0.744553f, 0.019470f);
  SkColorSpaceTransferFn transfer_fn = {2.1f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  ColorSpace color_spaces[kNumTests] = {
      ColorSpace(ColorSpace::PrimaryID::BT709,
                 ColorSpace::TransferID::IEC61966_2_1),
      ColorSpace(ColorSpace::PrimaryID::ADOBE_RGB,
                 ColorSpace::TransferID::IEC61966_2_1),
      ColorSpace(ColorSpace::PrimaryID::SMPTEST432_1,
                 ColorSpace::TransferID::LINEAR),
      ColorSpace(ColorSpace::PrimaryID::BT2020,
                 ColorSpace::TransferID::IEC61966_2_1),
      ColorSpace::CreateCustom(primary_matrix, transfer_fn),
  };
  sk_sp<SkColorSpace> sk_color_spaces[kNumTests] = {
      SkColorSpace::MakeSRGB(),
      SkColorSpace::MakeRGB(SkColorSpace::kSRGB_RenderTargetGamma,
                            SkColorSpace::kAdobeRGB_Gamut),
      SkColorSpace::MakeRGB(SkColorSpace::kLinear_RenderTargetGamma,
                            SkColorSpace::kDCIP3_D65_Gamut),
      SkColorSpace::MakeRGB(SkColorSpace::kSRGB_RenderTargetGamma,
                            SkColorSpace::kRec2020_Gamut),
      SkColorSpace::MakeRGB(transfer_fn, primary_matrix),
  };
  sk_sp<SkColorSpace> got_sk_color_spaces[kNumTests];
  for (size_t i = 0; i < kNumTests; ++i)
    got_sk_color_spaces[i] = color_spaces[i].ToSkColorSpace();
  for (size_t i = 0; i < kNumTests; ++i) {
    EXPECT_TRUE(SkColorSpace::Equals(got_sk_color_spaces[i].get(),
                                     sk_color_spaces[i].get()))
        << " on iteration i = " << i;
    // ToSkColorSpace should return the same thing every time.
    EXPECT_EQ(got_sk_color_spaces[i].get(),
              color_spaces[i].ToSkColorSpace().get())
        << " on iteration i = " << i;
    // But there is no cache within Skia, except for sRGB.
    // This test may start failing if this behavior changes.
    if (i != 0) {
      EXPECT_NE(got_sk_color_spaces[i].get(), sk_color_spaces[i].get())
          << " on iteration i = " << i;
    }
  }
}

}  // namespace
}  // namespace gfx
