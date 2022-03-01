// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_color_provider_utils.h"

#include "base/test/gtest_util.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"

using ChromeColorProviderUtilsTest = ::testing::Test;

TEST_F(ChromeColorProviderUtilsTest, ChromeColorIdNameTest) {
  EXPECT_EQ("kColorOmniboxBackground",
            ChromeColorIdName(kColorOmniboxBackground));
}
