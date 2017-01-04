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

  ICCProfile icc_profile_from_color_space =
      ICCProfile::FromColorSpace(color_space_from_icc_profile);
  EXPECT_TRUE(icc_profile == icc_profile_from_color_space);

  sk_sp<SkColorSpace> sk_color_space_from_color_space =
      color_space_from_icc_profile.ToSkColorSpace();

  ColorSpace color_space_from_sk_color_space =
      ColorSpace::FromSkColorSpace(sk_color_space_from_color_space);
  EXPECT_TRUE(color_space_from_icc_profile == color_space_from_sk_color_space);

  ICCProfile icc_profile_from_color_space_from_sk_color_space =
      ICCProfile::FromColorSpace(color_space_from_sk_color_space);
  EXPECT_TRUE(icc_profile == icc_profile_from_color_space_from_sk_color_space);
}

TEST(ICCProfile, SRGB) {
  ColorSpace color_space = ColorSpace::CreateSRGB();
  sk_sp<SkColorSpace> sk_color_space =
      SkColorSpace::MakeNamed(SkColorSpace::kSRGB_Named);

  ColorSpace color_space_from_sk_color_space =
      ColorSpace::FromSkColorSpace(sk_color_space);
  EXPECT_TRUE(color_space == color_space_from_sk_color_space);

  sk_sp<SkColorSpace> sk_color_space_from_color_space =
      color_space.ToSkColorSpace();
  EXPECT_TRUE(SkColorSpace::Equals(sk_color_space.get(),
                                   sk_color_space_from_color_space.get()));
}

}  // namespace gfx
