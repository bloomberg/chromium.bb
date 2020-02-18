// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/coordinate_conversion.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/wm/core/default_screen_position_client.h"

namespace wm {

typedef aura::test::AuraTestBase CoordinateConversionTest;

TEST_F(CoordinateConversionTest, ConvertRect) {
  DefaultScreenPositionClient screen_position_client;
  aura::client::SetScreenPositionClient(root_window(), &screen_position_client);
  aura::Window* w = aura::test::CreateTestWindowWithBounds(
      gfx::Rect(10, 20, 100, 200), root_window());

  gfx::Rect r1(10, 20, 100, 120);
  ConvertRectFromScreen(w, &r1);
  EXPECT_EQ("0,0 100x120", r1.ToString());

  gfx::Rect r2(0, 0, 100, 200);
  ConvertRectFromScreen(w, &r2);
  EXPECT_EQ("-10,-20 100x200", r2.ToString());

  gfx::Rect r3(30, 30, 100, 200);
  ConvertRectToScreen(w, &r3);
  EXPECT_EQ("40,50 100x200", r3.ToString());

  gfx::Rect r4(-10, -20, 100, 200);
  ConvertRectToScreen(w, &r4);
  EXPECT_EQ("0,0 100x200", r4.ToString());

  aura::client::SetScreenPositionClient(root_window(), nullptr);
}

}  // namespace wm
