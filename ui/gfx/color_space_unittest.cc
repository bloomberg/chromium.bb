// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

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

typedef std::tr1::tuple<ColorSpace::TransferID, size_t> TableTestData;

class ColorSpaceTableTest : public testing::TestWithParam<TableTestData> {};

TEST_P(ColorSpaceTableTest, ApproximateTransferFn) {
  ColorSpace::TransferID transfer_id = std::tr1::get<0>(GetParam());
  const size_t table_size = std::tr1::get<1>(GetParam());

  gfx::ColorSpace color_space(ColorSpace::PrimaryID::BT709, transfer_id);
  SkColorSpaceTransferFn tr_fn;
  SkColorSpaceTransferFn tr_fn_inv;
  bool result = color_space.GetTransferFunction(&tr_fn);
  CHECK(result);
  color_space.GetInverseTransferFunction(&tr_fn_inv);

  std::vector<float> x;
  std::vector<float> t;
  for (float v = 0; v <= 1.f; v += 1.f / table_size) {
    x.push_back(v);
    t.push_back(SkTransferFnEval(tr_fn, v));
  }

  SkColorSpaceTransferFn fn_approx;
  bool converged =
      SkApproximateTransferFn(x.data(), t.data(), x.size(), &fn_approx);
  EXPECT_TRUE(converged);

  for (size_t i = 0; i < x.size(); ++i) {
    float fn_approx_of_x = SkTransferFnEval(fn_approx, x[i]);
    EXPECT_NEAR(t[i], fn_approx_of_x, 2.f / 256.f);
    if (std::abs(t[i] - fn_approx_of_x) > 2.f / 256.f)
      break;
  }
}

ColorSpace::TransferID all_transfers[] = {
    ColorSpace::TransferID::GAMMA22,   ColorSpace::TransferID::GAMMA24,
    ColorSpace::TransferID::GAMMA28,   ColorSpace::TransferID::BT709,
    ColorSpace::TransferID::SMPTE240M, ColorSpace::TransferID::IEC61966_2_1,
    ColorSpace::TransferID::LINEAR};

size_t all_table_sizes[] = {512, 256, 128, 64, 16, 11, 8, 7, 6, 5, 4};

INSTANTIATE_TEST_CASE_P(A,
                        ColorSpaceTableTest,
                        testing::Combine(testing::ValuesIn(all_transfers),
                                         testing::ValuesIn(all_table_sizes)));

TEST(ColorSpace, ApproximateTransferFnBadMatch) {
  const float kStep = 1.f / 512.f;
  ColorSpace::TransferID transfer_ids[3] = {
      ColorSpace::TransferID::IEC61966_2_1, ColorSpace::TransferID::GAMMA22,
      ColorSpace::TransferID::BT709,
  };
  gfx::ColorSpace color_spaces[3];

  // The first iteration will have a perfect match. The second will be very
  // close. The third will converge, but with an error of ~7/256.
  for (size_t transfers_to_use = 1; transfers_to_use <= 3; ++transfers_to_use) {
    std::vector<float> x;
    std::vector<float> t;
    for (size_t c = 0; c < transfers_to_use; ++c) {
      color_spaces[c] =
          gfx::ColorSpace(ColorSpace::PrimaryID::BT709, transfer_ids[c]);
      SkColorSpaceTransferFn tr_fn;
      bool result = color_spaces[c].GetTransferFunction(&tr_fn);
      CHECK(result);

      for (float v = 0; v <= 1.f; v += kStep) {
        x.push_back(v);
        t.push_back(SkTransferFnEval(tr_fn, v));
      }
    }

    SkColorSpaceTransferFn fn_approx;
    bool converged =
        SkApproximateTransferFn(x.data(), t.data(), x.size(), &fn_approx);
    EXPECT_TRUE(converged);

    float expected_error = 1.5f / 256.f;
    if (transfers_to_use == 3)
      expected_error = 8.f / 256.f;

    for (size_t i = 0; i < x.size(); ++i) {
      float fn_approx_of_x = SkTransferFnEval(fn_approx, x[i]);
      EXPECT_NEAR(t[i], fn_approx_of_x, expected_error);
      if (std::abs(t[i] - fn_approx_of_x) > expected_error)
        break;
    }
  }
}

}  // namespace
}  // namespace gfx
