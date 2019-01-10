// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class SkiaUtilsTest : public testing::Test {};

// Tests converting a SkColorSpace to a gfx::ColorSpace
TEST_F(SkiaUtilsTest, SkColorSpaceToGfxColorSpace) {
  std::vector<sk_sp<SkColorSpace>> skia_color_spaces;

  skcms_TransferFunction transferFns[] = {
      SkNamedTransferFn::kLinear,
      SkNamedTransferFn::kSRGB,
  };

  skcms_Matrix3x3 gamuts[] = {
      SkNamedGamut::kSRGB,
      SkNamedGamut::kAdobeRGB,
      SkNamedGamut::kDCIP3,
      SkNamedGamut::kRec2020,
  };

  skia_color_spaces.push_back((SkColorSpace::MakeSRGB())->makeColorSpin());

  for (skcms_TransferFunction transferFn : transferFns) {
    for (skcms_Matrix3x3 gamut : gamuts) {
      skia_color_spaces.push_back(SkColorSpace::MakeRGB(transferFn, gamut));
    }
  }

  std::vector<gfx::ColorSpace> gfx_color_spaces;
  for (unsigned i = 0; i < skia_color_spaces.size(); i++) {
    gfx::ColorSpace color_space =
        SkColorSpaceToGfxColorSpace(skia_color_spaces[i]);
    ASSERT_TRUE(SkColorSpace::Equals(color_space.ToSkColorSpace().get(),
                                     skia_color_spaces[i].get()));
  }
}

}  // namespace blink
