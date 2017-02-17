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
