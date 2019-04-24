// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_screen_ozone.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"

namespace ui {

// This test ensures that PlatformScreen fetches display.
TEST(X11ScreenOzoneTest, FetchDisplay) {
  constexpr uint32_t kNumberOfDisplays = 1;

  std::unique_ptr<X11ScreenOzone> platform_screen =
      std::make_unique<X11ScreenOzone>();

  // Ensure there is only one display, which is the primary one.
  auto& all_displays = platform_screen->GetAllDisplays();
  EXPECT_EQ(all_displays.size(), kNumberOfDisplays);
}

}  // namespace ui
