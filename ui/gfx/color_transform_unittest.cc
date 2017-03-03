// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/test/icc_profiles.h"
#include "ui/gfx/transform.h"

namespace gfx {

// Internal functions, exposted for testing.
GFX_EXPORT Transform GetTransferMatrix(ColorSpace::MatrixID id);

ColorSpace::PrimaryID all_primaries[] = {
    ColorSpace::PrimaryID::BT709,        ColorSpace::PrimaryID::BT470M,
    ColorSpace::PrimaryID::BT470BG,      ColorSpace::PrimaryID::SMPTE170M,
    ColorSpace::PrimaryID::SMPTE240M,    ColorSpace::PrimaryID::FILM,
    ColorSpace::PrimaryID::BT2020,       ColorSpace::PrimaryID::SMPTEST428_1,
    ColorSpace::PrimaryID::SMPTEST431_2, ColorSpace::PrimaryID::SMPTEST432_1,
};

ColorSpace::TransferID all_transfers[] = {
    ColorSpace::TransferID::BT709,        ColorSpace::TransferID::GAMMA22,
    ColorSpace::TransferID::GAMMA28,      ColorSpace::TransferID::SMPTE170M,
    ColorSpace::TransferID::SMPTE240M,    ColorSpace::TransferID::LINEAR,
    ColorSpace::TransferID::LOG,          ColorSpace::TransferID::LOG_SQRT,
    ColorSpace::TransferID::IEC61966_2_4, ColorSpace::TransferID::BT1361_ECG,
    ColorSpace::TransferID::IEC61966_2_1, ColorSpace::TransferID::BT2020_10,
    ColorSpace::TransferID::BT2020_12,    ColorSpace::TransferID::SMPTEST2084,
    ColorSpace::TransferID::ARIB_STD_B67,
    // This one is weird as the non-linear numbers are not between 0 and 1.
    // TODO(hubbe): Test this separately.
    //  ColorSpace::TransferID::SMPTEST428_1,
};

ColorSpace::MatrixID all_matrices[] = {
    ColorSpace::MatrixID::RGB, ColorSpace::MatrixID::BT709,
    ColorSpace::MatrixID::FCC, ColorSpace::MatrixID::BT470BG,
    ColorSpace::MatrixID::SMPTE170M, ColorSpace::MatrixID::SMPTE240M,

    // YCOCG produces lots of negative values which isn't compatible with many
    // transfer functions.
    // TODO(hubbe): Test this separately.
    // ColorSpace::MatrixID::YCOCG,
    ColorSpace::MatrixID::BT2020_NCL, ColorSpace::MatrixID::BT2020_CL,
    ColorSpace::MatrixID::YDZDX,
};

ColorSpace::RangeID all_ranges[] = {ColorSpace::RangeID::FULL,
                                    ColorSpace::RangeID::LIMITED,
                                    ColorSpace::RangeID::DERIVED};

ColorTransform::Intent intents[] = {ColorTransform::Intent::INTENT_ABSOLUTE,
                                    ColorTransform::Intent::TEST_NO_OPT};

TEST(SimpleColorSpace, BT709toSRGB) {
  ColorSpace bt709 = ColorSpace::CreateREC709();
  ColorSpace sRGB = ColorSpace::CreateSRGB();
  std::unique_ptr<ColorTransform> t(ColorTransform::NewColorTransform(
      bt709, sRGB, ColorTransform::Intent::INTENT_ABSOLUTE));

  ColorTransform::TriStim tmp(16.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, 0.001f);
  EXPECT_NEAR(tmp.y(), 0.0f, 0.001f);
  EXPECT_NEAR(tmp.z(), 0.0f, 0.001f);

  tmp = ColorTransform::TriStim(235.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, 0.001f);
  EXPECT_NEAR(tmp.y(), 1.0f, 0.001f);
  EXPECT_NEAR(tmp.z(), 1.0f, 0.001f);

  // Test a blue color
  tmp = ColorTransform::TriStim(128.0f / 255.0f, 240.0f / 255.0f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_GT(tmp.z(), tmp.x());
  EXPECT_GT(tmp.z(), tmp.y());
}

TEST(SimpleColorSpace, TransferFnCancel) {
  ColorSpace::PrimaryID primary = ColorSpace::PrimaryID::BT709;
  ColorSpace::MatrixID matrix = ColorSpace::MatrixID::RGB;
  ColorSpace::RangeID range = ColorSpace::RangeID::FULL;

  // BT709 has a gamma of 2.2222 (with some adjustments)
  ColorSpace bt709(primary, ColorSpace::TransferID::BT709, matrix, range);

  // IEC61966_2_1 has the sRGB gamma of 2.4 (with some adjustments)
  ColorSpace srgb(primary, ColorSpace::TransferID::IEC61966_2_1, matrix, range);

  // gamma28 is a simple exponential
  ColorSpace gamma28(primary, ColorSpace::TransferID::GAMMA28, matrix, range);

  // gamma24 is a simple exponential
  ColorSpace gamma24(primary, ColorSpace::TransferID::GAMMA24, matrix, range);

  // BT709 source is common for video and sRGB destination is common for
  // monitors. The two transfer functions are very close, and should cancel
  // out (so the transfer between them should be the identity). This particular
  // case is important for power reasons.
  std::unique_ptr<ColorTransform> bt709_to_srgb(
      ColorTransform::NewColorTransform(
          bt709, srgb, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(bt709_to_srgb->NumberOfStepsForTesting(), 0u);

  // Gamma 2.8 isn't even close to BT709 and won't cancel out (so we will have
  // two steps in the transform -- to-linear and from-linear).
  std::unique_ptr<ColorTransform> bt709_to_gamma28(
      ColorTransform::NewColorTransform(
          bt709, gamma28, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(bt709_to_gamma28->NumberOfStepsForTesting(), 2u);

  // Gamma 2.4 is closer to BT709, but not close enough to actually cancel out.
  std::unique_ptr<ColorTransform> bt709_to_gamma24(
      ColorTransform::NewColorTransform(
          bt709, gamma24, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(bt709_to_gamma24->NumberOfStepsForTesting(), 2u);
}

TEST(SimpleColorSpace, SRGBFromICCAndNotICC) {
  float kEpsilon = 0.001f;
  ColorTransform::TriStim value_fromicc;
  ColorTransform::TriStim value_default;

  ICCProfile srgb_icc_profile = ICCProfileForTestingSRGB();
  ColorSpace srgb_fromicc = srgb_icc_profile.GetColorSpace();
  ColorSpace srgb_default = gfx::ColorSpace::CreateSRGB();
  ColorSpace xyzd50 = gfx::ColorSpace::CreateXYZD50();

  value_fromicc = value_default = ColorTransform::TriStim(0.1f, 0.5f, 0.9f);

  std::unique_ptr<ColorTransform> toxyzd50_fromicc(
      ColorTransform::NewColorTransform(
          srgb_fromicc, xyzd50, ColorTransform::Intent::INTENT_ABSOLUTE));
  // This will be converted to a transfer function and then linear transform.
  EXPECT_EQ(toxyzd50_fromicc->NumberOfStepsForTesting(), 2u);
  toxyzd50_fromicc->Transform(&value_fromicc, 1);

  std::unique_ptr<ColorTransform> toxyzd50_default(
      ColorTransform::NewColorTransform(
          srgb_default, xyzd50, ColorTransform::Intent::INTENT_ABSOLUTE));
  // This will have a transfer function and then linear transform.
  EXPECT_EQ(toxyzd50_default->NumberOfStepsForTesting(), 2u);
  toxyzd50_default->Transform(&value_default, 1);

  EXPECT_NEAR(value_fromicc.x(), value_default.x(), kEpsilon);
  EXPECT_NEAR(value_fromicc.y(), value_default.y(), kEpsilon);
  EXPECT_NEAR(value_fromicc.z(), value_default.z(), kEpsilon);

  value_fromicc = value_default = ColorTransform::TriStim(0.1f, 0.5f, 0.9f);

  std::unique_ptr<ColorTransform> fromxyzd50_fromicc(
      ColorTransform::NewColorTransform(
          xyzd50, srgb_fromicc, ColorTransform::Intent::INTENT_ABSOLUTE));
  fromxyzd50_fromicc->Transform(&value_fromicc, 1);

  std::unique_ptr<ColorTransform> fromxyzd50_default(
      ColorTransform::NewColorTransform(
          xyzd50, srgb_default, ColorTransform::Intent::INTENT_ABSOLUTE));
  fromxyzd50_default->Transform(&value_default, 1);

  EXPECT_NEAR(value_fromicc.x(), value_default.x(), kEpsilon);
  EXPECT_NEAR(value_fromicc.y(), value_default.y(), kEpsilon);
  EXPECT_NEAR(value_fromicc.z(), value_default.z(), kEpsilon);
}

TEST(SimpleColorSpace, BT709toSRGBICC) {
  ICCProfile srgb_icc = ICCProfileForTestingSRGB();
  ColorSpace bt709 = ColorSpace::CreateREC709();
  ColorSpace sRGB = srgb_icc.GetColorSpace();
  std::unique_ptr<ColorTransform> t(ColorTransform::NewColorTransform(
      bt709, sRGB, ColorTransform::Intent::INTENT_ABSOLUTE));

  ColorTransform::TriStim tmp(16.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, 0.001f);
  EXPECT_NEAR(tmp.y(), 0.0f, 0.001f);
  EXPECT_NEAR(tmp.z(), 0.0f, 0.001f);

  tmp = ColorTransform::TriStim(235.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, 0.001f);
  EXPECT_NEAR(tmp.y(), 1.0f, 0.001f);
  EXPECT_NEAR(tmp.z(), 1.0f, 0.001f);

  // Test a blue color
  tmp = ColorTransform::TriStim(128.0f / 255.0f, 240.0f / 255.0f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_GT(tmp.z(), tmp.x());
  EXPECT_GT(tmp.z(), tmp.y());
}

TEST(SimpleColorSpace, GetColorSpace) {
  ICCProfile srgb_icc = ICCProfileForTestingSRGB();
  ColorSpace sRGB = srgb_icc.GetColorSpace();
  ColorSpace sRGB2 = sRGB;
  const float kEpsilon = 1.5f / 255.f;

  // Prevent sRGB2 from using a cached ICC profile.
  sRGB2.icc_profile_id_ = 0;

  std::unique_ptr<ColorTransform> t(ColorTransform::NewColorTransform(
      sRGB, sRGB2, ColorTransform::Intent::INTENT_ABSOLUTE));

  ColorTransform::TriStim tmp(1.0f, 1.0f, 1.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, kEpsilon);
  EXPECT_NEAR(tmp.y(), 1.0f, kEpsilon);
  EXPECT_NEAR(tmp.z(), 1.0f, kEpsilon);

  tmp = ColorTransform::TriStim(1.0f, 0.0f, 0.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, kEpsilon);
  EXPECT_NEAR(tmp.y(), 0.0f, kEpsilon);
  EXPECT_NEAR(tmp.z(), 0.0f, kEpsilon);

  tmp = ColorTransform::TriStim(0.0f, 1.0f, 0.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, kEpsilon);
  EXPECT_NEAR(tmp.y(), 1.0f, kEpsilon);
  EXPECT_NEAR(tmp.z(), 0.0f, kEpsilon);

  tmp = ColorTransform::TriStim(0.0f, 0.0f, 1.0f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, kEpsilon);
  EXPECT_NEAR(tmp.y(), 0.0f, kEpsilon);
  EXPECT_NEAR(tmp.z(), 1.0f, kEpsilon);
}

TEST(SimpleColorSpace, UnknownVideoToSRGB) {
  // Invalid video spaces should be BT709.
  ColorSpace unknown = gfx::ColorSpace::CreateVideo(
      -1, -1, -1, gfx::ColorSpace::RangeID::LIMITED);
  ColorSpace sRGB = ColorSpace::CreateSRGB();
  std::unique_ptr<ColorTransform> t(ColorTransform::NewColorTransform(
      unknown, sRGB, ColorTransform::Intent::INTENT_PERCEPTUAL));

  ColorTransform::TriStim tmp(16.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 0.0f, 0.001f);
  EXPECT_NEAR(tmp.y(), 0.0f, 0.001f);
  EXPECT_NEAR(tmp.z(), 0.0f, 0.001f);

  tmp = ColorTransform::TriStim(235.0f / 255.0f, 0.5f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_NEAR(tmp.x(), 1.0f, 0.001f);
  EXPECT_NEAR(tmp.y(), 1.0f, 0.001f);
  EXPECT_NEAR(tmp.z(), 1.0f, 0.001f);

  // Test a blue color
  tmp = ColorTransform::TriStim(128.0f / 255.0f, 240.0f / 255.0f, 0.5f);
  t->Transform(&tmp, 1);
  EXPECT_GT(tmp.z(), tmp.x());
  EXPECT_GT(tmp.z(), tmp.y());
}

TEST(SimpleColorSpace, ToUndefined) {
  ColorSpace null;
  ColorSpace nonnull = gfx::ColorSpace::CreateSRGB();
  // Video should have 1 step: YUV to RGB.
  // Anything else should have 0 steps.
  ColorSpace video = gfx::ColorSpace::CreateREC709();
  std::unique_ptr<ColorTransform> video_to_null(
      ColorTransform::NewColorTransform(
          video, null, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(video_to_null->NumberOfStepsForTesting(), 1u);

  // Test with an ICC profile that can't be represented as matrix+transfer.
  ColorSpace luttrcicc = ICCProfileForTestingNoAnalyticTrFn().GetColorSpace();
  std::unique_ptr<ColorTransform> luttrcicc_to_null(
      ColorTransform::NewColorTransform(
          luttrcicc, null, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(luttrcicc_to_null->NumberOfStepsForTesting(), 0u);
  std::unique_ptr<ColorTransform> luttrcicc_to_nonnull(
      ColorTransform::NewColorTransform(
          luttrcicc, nonnull, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_GT(luttrcicc_to_nonnull->NumberOfStepsForTesting(), 0u);

  // Test with an ICC profile that can.
  ColorSpace adobeicc = ICCProfileForTestingAdobeRGB().GetColorSpace();
  std::unique_ptr<ColorTransform> adobeicc_to_null(
      ColorTransform::NewColorTransform(
          adobeicc, null, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(adobeicc_to_null->NumberOfStepsForTesting(), 0u);
  std::unique_ptr<ColorTransform> adobeicc_to_nonnull(
      ColorTransform::NewColorTransform(
          adobeicc, nonnull, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_GT(adobeicc_to_nonnull->NumberOfStepsForTesting(), 0u);

  // And with something analytic.
  ColorSpace srgb = gfx::ColorSpace::CreateXYZD50();
  std::unique_ptr<ColorTransform> srgb_to_null(
      ColorTransform::NewColorTransform(
          srgb, null, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(srgb_to_null->NumberOfStepsForTesting(), 0u);
  std::unique_ptr<ColorTransform> srgb_to_nonnull(
      ColorTransform::NewColorTransform(
          srgb, nonnull, ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_GT(srgb_to_nonnull->NumberOfStepsForTesting(), 0u);
}

TEST(SimpleColorSpace, DefaultToSRGB) {
  // The default value should do no transformation, regardless of destination.
  ColorSpace unknown;
  std::unique_ptr<ColorTransform> t1(ColorTransform::NewColorTransform(
      unknown, ColorSpace::CreateSRGB(),
      ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(t1->NumberOfStepsForTesting(), 0u);
  std::unique_ptr<ColorTransform> t2(ColorTransform::NewColorTransform(
      unknown, ColorSpace::CreateXYZD50(),
      ColorTransform::Intent::INTENT_PERCEPTUAL));
  EXPECT_EQ(t2->NumberOfStepsForTesting(), 0u);
}

// This tests to make sure that we don't emit the "if" or "pow" parts of a
// transfer function unless necessary.
TEST(SimpleColorSpace, ShaderSourceTrFnOptimizations) {
  SkMatrix44 primaries;
  gfx::ColorSpace::CreateSRGB().GetPrimaryMatrix(&primaries);

  SkColorSpaceTransferFn fn_no_pow_no_if = {
      1.f, 2.f, 0.f, 1.f, 0.f, 0.f, 0.f,
  };
  SkColorSpaceTransferFn fn_no_pow_yes_if = {
      1.f, 2.f, 0.f, 1.f, 0.5f, 0.f, 0.f,
  };
  SkColorSpaceTransferFn fn_yes_pow_no_if = {
      2.f, 2.f, 0.f, 1.f, 0.f, 0.f, 0.f,
  };
  SkColorSpaceTransferFn fn_yes_pow_yes_if = {
      2.f, 2.f, 0.f, 1.f, 0.5f, 0.f, 0.f,
  };

  gfx::ColorSpace src;
  gfx::ColorSpace dst = gfx::ColorSpace::CreateXYZD50();
  std::string shader_string;

  src = gfx::ColorSpace::CreateCustom(primaries, fn_no_pow_no_if);
  shader_string = ColorTransform::NewColorTransform(
                      src, dst, ColorTransform::Intent::INTENT_PERCEPTUAL)
                      ->GetShaderSource();
  EXPECT_EQ(shader_string.find("if ("), std::string::npos);
  EXPECT_EQ(shader_string.find("pow("), std::string::npos);

  src = gfx::ColorSpace::CreateCustom(primaries, fn_no_pow_yes_if);
  shader_string = ColorTransform::NewColorTransform(
                      src, dst, ColorTransform::Intent::INTENT_PERCEPTUAL)
                      ->GetShaderSource();
  EXPECT_NE(shader_string.find("if ("), std::string::npos);
  EXPECT_EQ(shader_string.find("pow("), std::string::npos);

  src = gfx::ColorSpace::CreateCustom(primaries, fn_yes_pow_no_if);
  shader_string = ColorTransform::NewColorTransform(
                      src, dst, ColorTransform::Intent::INTENT_PERCEPTUAL)
                      ->GetShaderSource();
  EXPECT_EQ(shader_string.find("if ("), std::string::npos);
  EXPECT_NE(shader_string.find("pow("), std::string::npos);

  src = gfx::ColorSpace::CreateCustom(primaries, fn_yes_pow_yes_if);
  shader_string = ColorTransform::NewColorTransform(
                      src, dst, ColorTransform::Intent::INTENT_PERCEPTUAL)
                      ->GetShaderSource();
  EXPECT_NE(shader_string.find("if ("), std::string::npos);
  EXPECT_NE(shader_string.find("pow("), std::string::npos);
}

// Note: This is not actually "testing" anything -- the goal of this test is to
// to make reviewing shader code simpler by giving an example of the resulting
// shader source. This should be updated whenever shader generation is updated.
// This test produces slightly different results on Android.
#if defined(OS_ANDROID)
#define MAYBE_SampleShaderSource DISABLED_SampleShaderSource
#else
#define MAYBE_SampleShaderSource SampleShaderSource
#endif
TEST(SimpleColorSpace, MAYBE_SampleShaderSource) {
  ColorSpace bt709 = ColorSpace::CreateREC709();
  ColorSpace output(ColorSpace::PrimaryID::BT2020,
                    ColorSpace::TransferID::GAMMA28);
  std::string source =
      ColorTransform::NewColorTransform(
          bt709, output, ColorTransform::Intent::INTENT_PERCEPTUAL)
          ->GetShaderSource();
  std::string expected =
      "vec3 DoColorConversion(vec3 color) {\n"
      "  color = mat3(1.16438353e+00, 1.16438353e+00, 1.16438353e+00,\n"
      "               -2.28029018e-09, -2.13248596e-01, 2.11240172e+00,\n"
      "               1.79274118e+00, -5.32909274e-01, -5.96049432e-10) "
      "* color;\n"
      "  color += vec3(-9.69429970e-01, 3.00019622e-01, -1.12926030e+00);\n"
      "  if (color.r < 4.04499359e-02)\n"
      "    color.r = 7.73993805e-02 * color.r;\n"
      "  else\n"
      "    color.r = pow(9.47867334e-01 * color.r + 5.21326549e-02, "
      "2.40000010e+00);\n"
      "  if (color.g < 4.04499359e-02)\n"
      "    color.g = 7.73993805e-02 * color.g;\n"
      "  else\n"
      "    color.g = pow(9.47867334e-01 * color.g + 5.21326549e-02, "
      "2.40000010e+00);\n"
      "  if (color.b < 4.04499359e-02)\n"
      "    color.b = 7.73993805e-02 * color.b;\n"
      "  else\n"
      "    color.b = pow(9.47867334e-01 * color.b + 5.21326549e-02, "
      "2.40000010e+00);\n"
      "  color = mat3(6.27403915e-01, 6.90973178e-02, 1.63914412e-02,\n"
      "               3.29283148e-01, 9.19540286e-01, 8.80132914e-02,\n"
      "               4.33131084e-02, 1.13623003e-02, 8.95595253e-01) "
      "* color;\n"
      "  color.r = pow(color.r, 3.57142866e-01);\n"
      "  color.g = pow(color.g, 3.57142866e-01);\n"
      "  color.b = pow(color.b, 3.57142866e-01);\n"
      "  return color;\n"
      "}\n";
  EXPECT_EQ(source, expected);
}

class TransferTest : public testing::TestWithParam<ColorSpace::TransferID> {};

TEST_P(TransferTest, basicTest) {
  gfx::ColorSpace space_with_transfer(ColorSpace::PrimaryID::BT709, GetParam(),
                                      ColorSpace::MatrixID::RGB,
                                      ColorSpace::RangeID::FULL);
  gfx::ColorSpace space_linear(
      ColorSpace::PrimaryID::BT709, ColorSpace::TransferID::LINEAR,
      ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);

  std::unique_ptr<ColorTransform> to_linear(ColorTransform::NewColorTransform(
      space_with_transfer, space_linear,
      ColorTransform::Intent::INTENT_ABSOLUTE));

  std::unique_ptr<ColorTransform> from_linear(ColorTransform::NewColorTransform(
      space_linear, space_with_transfer,
      ColorTransform::Intent::INTENT_ABSOLUTE));

  // The transforms will ahve 1 or 0 steps (0 for linear).
  size_t expected_steps = 1u;
  if (GetParam() == ColorSpace::TransferID::LINEAR)
    expected_steps = 0u;
  EXPECT_EQ(to_linear->NumberOfStepsForTesting(), expected_steps);
  EXPECT_EQ(from_linear->NumberOfStepsForTesting(), expected_steps);

  for (float x = 0.0f; x <= 1.0f; x += 1.0f / 128.0f) {
    ColorTransform::TriStim tristim(x, x, x);
    to_linear->Transform(&tristim, 1);
    from_linear->Transform(&tristim, 1);
    EXPECT_NEAR(x, tristim.x(), 0.001f);
  }
}

INSTANTIATE_TEST_CASE_P(ColorSpace,
                        TransferTest,
                        testing::ValuesIn(all_transfers));

typedef std::tr1::tuple<ColorSpace::PrimaryID,
                        ColorSpace::TransferID,
                        ColorSpace::MatrixID,
                        ColorSpace::RangeID,
                        ColorTransform::Intent>
    ColorSpaceTestData;

class ColorSpaceTest : public testing::TestWithParam<ColorSpaceTestData> {
 public:
  ColorSpaceTest()
      : color_space_(std::tr1::get<0>(GetParam()),
                     std::tr1::get<1>(GetParam()),
                     std::tr1::get<2>(GetParam()),
                     std::tr1::get<3>(GetParam())),
        intent_(std::tr1::get<4>(GetParam())) {}

 protected:
  ColorSpace color_space_;
  ColorTransform::Intent intent_;
};

TEST_P(ColorSpaceTest, testNullTransform) {
  std::unique_ptr<ColorTransform> t(
      ColorTransform::NewColorTransform(color_space_, color_space_, intent_));
  ColorTransform::TriStim tristim(0.4f, 0.5f, 0.6f);
  t->Transform(&tristim, 1);
  EXPECT_NEAR(tristim.x(), 0.4f, 0.001f);
  EXPECT_NEAR(tristim.y(), 0.5f, 0.001f);
  EXPECT_NEAR(tristim.z(), 0.6f, 0.001f);
}

TEST_P(ColorSpaceTest, toXYZandBack) {
  std::unique_ptr<ColorTransform> t1(ColorTransform::NewColorTransform(
      color_space_, ColorSpace::CreateXYZD50(), intent_));
  std::unique_ptr<ColorTransform> t2(ColorTransform::NewColorTransform(
      ColorSpace::CreateXYZD50(), color_space_, intent_));
  ColorTransform::TriStim tristim(0.4f, 0.5f, 0.6f);
  t1->Transform(&tristim, 1);
  t2->Transform(&tristim, 1);
  EXPECT_NEAR(tristim.x(), 0.4f, 0.001f);
  EXPECT_NEAR(tristim.y(), 0.5f, 0.001f);
  EXPECT_NEAR(tristim.z(), 0.6f, 0.001f);
}

INSTANTIATE_TEST_CASE_P(
    A,
    ColorSpaceTest,
    testing::Combine(testing::ValuesIn(all_primaries),
                     testing::ValuesIn(all_transfers),
                     testing::Values(ColorSpace::MatrixID::BT709),
                     testing::Values(ColorSpace::RangeID::LIMITED),
                     testing::ValuesIn(intents)));

INSTANTIATE_TEST_CASE_P(
    B,
    ColorSpaceTest,
    testing::Combine(testing::Values(ColorSpace::PrimaryID::BT709),
                     testing::ValuesIn(all_transfers),
                     testing::ValuesIn(all_matrices),
                     testing::ValuesIn(all_ranges),
                     testing::ValuesIn(intents)));

INSTANTIATE_TEST_CASE_P(
    C,
    ColorSpaceTest,
    testing::Combine(testing::ValuesIn(all_primaries),
                     testing::Values(ColorSpace::TransferID::BT709),
                     testing::ValuesIn(all_matrices),
                     testing::ValuesIn(all_ranges),
                     testing::ValuesIn(intents)));
}  // namespace
