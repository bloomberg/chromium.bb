// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/resources.h"

#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

// TODO(sergeyu): Resources loading doesn't work yet on OSX. Fix it and enable
// the test.
#if !defined(OS_MACOSX)
#define MAYBE_ProductName ProductName
#else  // !defined(OS_MACOSX)
#define MAYBE_ProductName DISABLED_ProductName
#endif  // defined(OS_MACOSX)
TEST(Resources, MAYBE_ProductName) {
#if defined(GOOGLE_CHROME_BUILD)
  std::string expected_product_name = "Chrome Remote Desktop";
#else  // defined(GOOGLE_CHROME_BUILD)
  std::string expected_product_name = "Chromoting";
#endif  // !defined(GOOGLE_CHROME_BUILD)
  ASSERT_TRUE(LoadResources("en-US"));
  EXPECT_EQ(expected_product_name,
            l10n_util::GetStringUTF8(IDR_REMOTING_PRODUCT_NAME));
  ui::ResourceBundle::CleanupSharedInstance();
}

}  // namespace remoting
