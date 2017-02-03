// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

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

}  // namespace
}  // namespace gfx
