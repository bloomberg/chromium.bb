// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/resources.h"

#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class ResourcesTest : public testing::Test {
 protected:
  ResourcesTest(): resources_available_(false) {
  }

  virtual void SetUp() OVERRIDE {
    resources_available_ = LoadResources("en-US");
  }

  virtual void TearDown() OVERRIDE {
    UnloadResources();
  }

  bool resources_available_;
};

// TODO(alexeypa): Reenable the test once http://crbug.com/269143 (ChromeOS) and
// http://crbug.com/268043 (MacOS) are fixed.
TEST_F(ResourcesTest, DISABLED_ProductName) {
#if defined(GOOGLE_CHROME_BUILD)
  std::string expected_product_name = "Chrome Remote Desktop";
#else  // defined(GOOGLE_CHROME_BUILD)
  std::string expected_product_name = "Chromoting";
#endif  // !defined(GOOGLE_CHROME_BUILD)

  // Chrome-style i18n is not used on Windows.
#if defined(OS_WIN)
  EXPECT_FALSE(resources_available_);
#else
  EXPECT_TRUE(resources_available_);
#endif

  if (resources_available_) {
    EXPECT_EQ(expected_product_name,
              l10n_util::GetStringUTF8(IDS_PRODUCT_NAME));
  }
}

}  // namespace remoting
