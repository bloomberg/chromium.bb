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
    EXPECT_NEAR(t[i], fn_approx_of_x, 3.f / 256.f);
    if (std::abs(t[i] - fn_approx_of_x) > 3.f / 256.f)
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

TEST(ColorSpace, ApproximateTransferFnClamped) {
  // These data represent a transfer function that is clamped at the high
  // end of its domain. It comes from the color profile attached to
  // https://crbug.com/750459
  float t[] = {
      0.000000f, 0.000305f, 0.000610f, 0.000916f, 0.001221f, 0.001511f,
      0.001816f, 0.002121f, 0.002426f, 0.002731f, 0.003037f, 0.003601f,
      0.003937f, 0.004303f, 0.004685f, 0.005081f, 0.005509f, 0.005951f,
      0.006409f, 0.006882f, 0.007385f, 0.007904f, 0.008438f, 0.009003f,
      0.009583f, 0.010193f, 0.010819f, 0.011460f, 0.012131f, 0.012818f,
      0.013535f, 0.014267f, 0.015030f, 0.015808f, 0.016617f, 0.017456f,
      0.018296f, 0.019181f, 0.020081f, 0.021012f, 0.021958f, 0.022934f,
      0.023926f, 0.024949f, 0.026001f, 0.027070f, 0.028168f, 0.029297f,
      0.030442f, 0.031617f, 0.032822f, 0.034058f, 0.035309f, 0.036591f,
      0.037903f, 0.039231f, 0.040604f, 0.041993f, 0.043412f, 0.044846f,
      0.046326f, 0.047822f, 0.049348f, 0.050904f, 0.052491f, 0.054108f,
      0.055756f, 0.057420f, 0.059113f, 0.060853f, 0.062608f, 0.064393f,
      0.066209f, 0.068055f, 0.069932f, 0.071839f, 0.073762f, 0.075731f,
      0.077729f, 0.079759f, 0.081804f, 0.083894f, 0.086015f, 0.088167f,
      0.090333f, 0.092546f, 0.094789f, 0.097063f, 0.099367f, 0.101701f,
      0.104067f, 0.106477f, 0.108904f, 0.111360f, 0.113863f, 0.116381f,
      0.118944f, 0.121538f, 0.124163f, 0.126818f, 0.129519f, 0.132235f,
      0.134997f, 0.137789f, 0.140612f, 0.143465f, 0.146365f, 0.149279f,
      0.152239f, 0.155230f, 0.158267f, 0.161318f, 0.164416f, 0.167544f,
      0.170718f, 0.173907f, 0.177142f, 0.180407f, 0.183719f, 0.187045f,
      0.190433f, 0.193835f, 0.197284f, 0.200763f, 0.204273f, 0.207813f,
      0.211398f, 0.215030f, 0.218692f, 0.222385f, 0.226108f, 0.229877f,
      0.233677f, 0.237522f, 0.241382f, 0.245304f, 0.249256f, 0.253239f,
      0.257252f, 0.261311f, 0.265415f, 0.269551f, 0.273716f, 0.277928f,
      0.282170f, 0.286458f, 0.290776f, 0.295140f, 0.299535f, 0.303975f,
      0.308446f, 0.312947f, 0.317494f, 0.322087f, 0.326711f, 0.331380f,
      0.336080f, 0.340826f, 0.345602f, 0.350423f, 0.355291f, 0.360174f,
      0.365118f, 0.370092f, 0.375113f, 0.380163f, 0.385260f, 0.390387f,
      0.395560f, 0.400778f, 0.406027f, 0.411322f, 0.416663f, 0.422034f,
      0.427451f, 0.432898f, 0.438392f, 0.443931f, 0.449500f, 0.455116f,
      0.460777f, 0.466468f, 0.472221f, 0.477989f, 0.483818f, 0.489677f,
      0.495583f, 0.501518f, 0.507500f, 0.513527f, 0.519600f, 0.525719f,
      0.531868f, 0.538064f, 0.544289f, 0.550576f, 0.556893f, 0.563256f,
      0.569650f, 0.576104f, 0.582589f, 0.589120f, 0.595697f, 0.602304f,
      0.608972f, 0.615671f, 0.622415f, 0.629206f, 0.636027f, 0.642908f,
      0.649821f, 0.656779f, 0.663783f, 0.670832f, 0.677913f, 0.685054f,
      0.692226f, 0.699443f, 0.706706f, 0.714015f, 0.721370f, 0.728771f,
      0.736202f, 0.743694f, 0.751217f, 0.758785f, 0.766400f, 0.774060f,
      0.781765f, 0.789517f, 0.797314f, 0.805158f, 0.813031f, 0.820966f,
      0.828946f, 0.836957f, 0.845029f, 0.853132f, 0.861280f, 0.869490f,
      0.877729f, 0.886015f, 0.894362f, 0.902739f, 0.911162f, 0.919631f,
      0.928161f, 0.936721f, 0.945327f, 0.953994f, 0.962692f, 0.971435f,
      0.980240f, 0.989075f, 0.997955f, 1.000000f,
  };
  std::vector<float> x;
  for (size_t v = 0; v < 256; ++v)
    x.push_back(v / 255.f);

  SkColorSpaceTransferFn fn_approx;
  bool converged = SkApproximateTransferFn(x.data(), t, x.size(), &fn_approx);
  EXPECT_TRUE(converged);

  // The approximation should be nearly exact.
  float expected_error = 1.f / 4096.f;
  for (size_t i = 0; i < x.size(); ++i) {
    float fn_approx_of_x = SkTransferFnEval(fn_approx, x[i]);
    EXPECT_NEAR(t[i], fn_approx_of_x, expected_error);
    if (std::abs(t[i] - fn_approx_of_x) > expected_error)
      break;
  }
}

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
