// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

// A targeted test for GetFallbackFontFamilies on Mac. It uses a system API that
// only became publicly available in the 10.8 SDK. This test is to ensure it
// behaves sensibly on all supported OS versions.
GTEST_TEST(FontFallbackMacTest, GetFallbackFontFamilies) {
  std::vector<std::string> fallback_families = GetFallbackFontFamilies("Arial");
  // If there is only one fallback, it means the only fallback is the font
  // itself.
  EXPECT_LT(1u, fallback_families.size());
}

}  // namespace gfx
