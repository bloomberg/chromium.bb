// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/test/icc_profiles.h"

namespace gfx {

TEST(ICCProfile, Conversions) {
  ICCProfile icc_profile = ICCProfileForTestingColorSpin();
  ColorSpace color_space_from_icc_profile = icc_profile.GetColorSpace();

  ICCProfile icc_profile_from_color_space;
  bool result =
      color_space_from_icc_profile.GetICCProfile(&icc_profile_from_color_space);
  EXPECT_TRUE(result);
  EXPECT_TRUE(icc_profile == icc_profile_from_color_space);
}

TEST(ICCProfile, SRGB) {
  ICCProfile icc_profile = ICCProfileForTestingSRGB();
  ColorSpace color_space = ColorSpace::CreateSRGB();
  sk_sp<SkColorSpace> sk_color_space = SkColorSpace::MakeSRGB();

  // The ICC profile parser should note that this is SRGB.
  EXPECT_EQ(icc_profile.GetColorSpace().ToSkColorSpace().get(),
            sk_color_space.get());
  // The parametric generating code should recognize that this is SRGB.
  EXPECT_EQ(icc_profile.GetParametricColorSpace().ToSkColorSpace().get(),
            sk_color_space.get());
  // The generated color space should recognize that this is SRGB.
  EXPECT_EQ(color_space.ToSkColorSpace().get(), sk_color_space.get());
}

TEST(ICCProfile, Equality) {
  ICCProfile spin_profile = ICCProfileForTestingColorSpin();
  ICCProfile adobe_profile = ICCProfileForTestingAdobeRGB();
  EXPECT_TRUE(spin_profile == spin_profile);
  EXPECT_FALSE(spin_profile != spin_profile);
  EXPECT_FALSE(spin_profile == adobe_profile);
  EXPECT_TRUE(spin_profile != adobe_profile);

  gfx::ColorSpace spin_space = spin_profile.GetColorSpace();
  gfx::ColorSpace adobe_space = adobe_profile.GetColorSpace();
  EXPECT_TRUE(spin_space == spin_space);
  EXPECT_FALSE(spin_space != spin_space);
  EXPECT_FALSE(spin_space == adobe_space);
  EXPECT_TRUE(spin_space != adobe_space);

  ICCProfile temp;
  bool get_icc_result = false;

  get_icc_result = spin_space.GetICCProfile(&temp);
  EXPECT_TRUE(get_icc_result);
  EXPECT_TRUE(spin_profile == temp);
  EXPECT_FALSE(spin_profile != temp);

  get_icc_result = adobe_space.GetICCProfile(&temp);
  EXPECT_TRUE(get_icc_result);
  EXPECT_FALSE(spin_profile == temp);
  EXPECT_TRUE(spin_profile != temp);

  EXPECT_TRUE(!!spin_space.ToSkColorSpace());
  EXPECT_TRUE(!!adobe_space.ToSkColorSpace());
  EXPECT_FALSE(SkColorSpace::Equals(
      spin_space.ToSkColorSpace().get(),
      adobe_space.ToSkColorSpace().get()));
}

TEST(ICCProfile, ParametricVersusExact) {
  // This ICC profile has three transfer functions that differ enough that the
  // parametric color space is considered inaccurate.
  ICCProfile multi_tr_fn = ICCProfileForTestingNoAnalyticTrFn();
  EXPECT_NE(multi_tr_fn.GetColorSpace(), multi_tr_fn.GetParametricColorSpace());

  ICCProfile multi_tr_fn_color_space;
  EXPECT_TRUE(
      multi_tr_fn.GetColorSpace().GetICCProfile(&multi_tr_fn_color_space));
  EXPECT_EQ(multi_tr_fn_color_space, multi_tr_fn);

  ICCProfile multi_tr_fn_parametric_color_space;
  EXPECT_TRUE(multi_tr_fn.GetParametricColorSpace().GetICCProfile(
      &multi_tr_fn_parametric_color_space));
  EXPECT_NE(multi_tr_fn_parametric_color_space, multi_tr_fn);

  // This ICC profile has a transfer function with T(1) that is greater than 1
  // in the approximation, but is still close enough to be considered accurate.
  ICCProfile overshoot = ICCProfileForTestingOvershoot();
  EXPECT_EQ(overshoot.GetColorSpace(), overshoot.GetParametricColorSpace());

  ICCProfile overshoot_color_space;
  EXPECT_TRUE(overshoot.GetColorSpace().GetICCProfile(&overshoot_color_space));
  EXPECT_EQ(overshoot_color_space, overshoot);

  ICCProfile overshoot_parametric_color_space;
  EXPECT_TRUE(overshoot.GetParametricColorSpace().GetICCProfile(
      &overshoot_parametric_color_space));
  EXPECT_EQ(overshoot_parametric_color_space, overshoot);

  // This ICC profile is precisely represented by the parametric color space.
  ICCProfile accurate = ICCProfileForTestingAdobeRGB();
  EXPECT_EQ(accurate.GetColorSpace(), accurate.GetParametricColorSpace());

  ICCProfile accurate_color_space;
  EXPECT_TRUE(accurate.GetColorSpace().GetICCProfile(&accurate_color_space));
  EXPECT_EQ(accurate_color_space, accurate);

  ICCProfile accurate_parametric_color_space;
  EXPECT_TRUE(accurate.GetParametricColorSpace().GetICCProfile(
      &accurate_parametric_color_space));
  EXPECT_EQ(accurate_parametric_color_space, accurate);

  // This ICC profile has only an A2B representation. We cannot create an
  // SkColorSpaceXform to A2B only ICC profiles, so this should be marked
  // as invalid.
  ICCProfile a2b = ICCProfileForTestingA2BOnly();
  EXPECT_FALSE(a2b.GetColorSpace().IsValid());
}

TEST(ICCProfile, GarbageData) {
  std::vector<char> bad_data(10 * 1024);
  const char* bad_data_string = "deadbeef";
  for (size_t i = 0; i < bad_data.size(); ++i)
    bad_data[i] = bad_data_string[i % 8];
  ICCProfile garbage_profile =
      ICCProfile::FromData(bad_data.data(), bad_data.size());
  EXPECT_FALSE(garbage_profile.IsValid());
  EXPECT_FALSE(garbage_profile.GetColorSpace().IsValid());
  EXPECT_FALSE(garbage_profile.GetParametricColorSpace().IsValid());

  ICCProfile default_ctor_profile;
  EXPECT_FALSE(default_ctor_profile.IsValid());
  EXPECT_FALSE(default_ctor_profile.GetColorSpace().IsValid());
  EXPECT_FALSE(default_ctor_profile.GetParametricColorSpace().IsValid());
}

}  // namespace gfx
