// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/fonts/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/fuchsia/service_directory_client.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkFontMgr_fuchsia.h"

namespace skia {

// Tests for SkFontMgr_Fuchsia in Skia.
class FuchsiaFontManagerTest : public testing::Test {
 public:
  FuchsiaFontManagerTest() {
    font_manager_ = SkFontMgr_New_Fuchsia(
        base::fuchsia::ServiceDirectoryClient::ForCurrentProcess()
            ->ConnectToServiceSync<fuchsia::fonts::Provider>());
  }

 protected:
  sk_sp<SkFontMgr> font_manager_;
};

// Verify that SkTypeface objects are cached.
// TODO(https://crbug.com/931333): Currently font provider returns the same
// font for sans and serif when used with the default font config. Update this
// test to use the fonts //third_party/test_fonts.
TEST_F(FuchsiaFontManagerTest, DISABLED_Caching) {
  sk_sp<SkTypeface> sans(
      font_manager_->matchFamilyStyle("sans", SkFontStyle()));
  EXPECT_TRUE(sans);

  sk_sp<SkTypeface> sans2(
      font_manager_->matchFamilyStyle("sans", SkFontStyle()));

  // Expect that the same SkTypeface is returned for both requests.
  EXPECT_EQ(sans.get(), sans2.get());

  // Request serif and verify that a different SkTypeface is returned.
  sk_sp<SkTypeface> serif(
      font_manager_->matchFamilyStyle("serif", SkFontStyle()));
  EXPECT_NE(sans.get(), serif.get());
}

// Verify that SkTypeface can outlive the manager.
TEST_F(FuchsiaFontManagerTest, TypefaceOutlivesManager) {
  sk_sp<SkTypeface> sans(
      font_manager_->matchFamilyStyle("sans", SkFontStyle()));
  font_manager_.reset();
}

// Verify that we can query a font after releasing a previous instance.
TEST_F(FuchsiaFontManagerTest, ReleaseThenCreateAgain) {
  sk_sp<SkTypeface> serif(
      font_manager_->matchFamilyStyle("serif", SkFontStyle()));
  EXPECT_TRUE(serif);
  serif.reset();

  sk_sp<SkTypeface> serif2(
      font_manager_->matchFamilyStyle("serif", SkFontStyle()));
  EXPECT_TRUE(serif2);
}

}  // namespace skia
