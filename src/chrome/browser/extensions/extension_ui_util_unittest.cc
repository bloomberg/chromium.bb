// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_ui_util.h"

#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/image_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// This test verifies that the assumed default color of the toolbar
// doesn't change, and if it does, we update the default value. We
// need this test at the browser level, since the lower levels where
// we use this value don't have access to the ThemeService.
//
// See ThemeProperties::GetDefaultColor() for the definition of the
// default color.
TEST(ExtensionUiUtilTest, CheckDefaultToolbarColor) {
  TestExtensionEnvironment env;
  TestingProfile profile;
  const ui::ThemeProvider& provider =
      ThemeService::GetThemeProviderForProfile(&profile);
  EXPECT_EQ(image_util::kDefaultToolbarColor,
            provider.GetColor(ThemeProperties::COLOR_TOOLBAR))
      << "Please update image_util::kDefaultToolbarColor to the new value";
}

}  // namespace extensions
