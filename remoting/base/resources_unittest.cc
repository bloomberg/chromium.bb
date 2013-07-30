// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/resources.h"

#include "remoting/base/common_resources.h"
#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

// TODO(sergeyu): Resources loading doesn't work yet on OSX. Fix it and enable
// the test.
#if !defined(OS_MACOSX)
#define MAYBE_ProductName ProductName
#define MAYBE_ProductLogo ProductLogo
#else  // !defined(OS_MACOSX)
#define MAYBE_ProductName DISABLED_ProductName
#define MAYBE_ProductLogo DISABLED_ProductLogo
#endif  // defined(OS_MACOSX)

class ResourcesTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(LoadResources("en-US"));
  }

  virtual void TearDown() OVERRIDE {
    ui::ResourceBundle::CleanupSharedInstance();
  }
};

TEST_F(ResourcesTest, MAYBE_ProductName) {
#if defined(GOOGLE_CHROME_BUILD)
  std::string expected_product_name = "Chrome Remote Desktop";
#else  // defined(GOOGLE_CHROME_BUILD)
  std::string expected_product_name = "Chromoting";
#endif  // !defined(GOOGLE_CHROME_BUILD)
  EXPECT_EQ(expected_product_name,
            l10n_util::GetStringUTF8(IDR_PRODUCT_NAME));
}

TEST_F(ResourcesTest, MAYBE_ProductLogo) {
  gfx::Image logo16 = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_PRODUCT_LOGO_16);
  EXPECT_FALSE(logo16.IsEmpty());
  gfx::Image logo32 = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_PRODUCT_LOGO_32);
  EXPECT_FALSE(logo32.IsEmpty());
}

}  // namespace remoting
