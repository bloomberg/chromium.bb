// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/server_util.h"
#include "components/exo/test/exo_test_base.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"

namespace exo {

using ServerUtilTest = test::ExoTestBase;

TEST_F(ServerUtilTest, GetWorkAreaInsetsInClientPixel) {
  UpdateDisplay("3000x2000*2.25,1920x1080");
  auto display = display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Insets insets = wayland::GetWorkAreaInsetsInClientPixel(
      display, 2.25f, gfx::Size(3000, 2000), display.work_area());
  EXPECT_EQ(gfx::Insets(0, 0, 128, 0), insets);

  auto secondary_display = GetSecondaryDisplay();
  gfx::Size secondary_size(1920, 1080);
  gfx::Size secondary_size_in_client_pixel =
      gfx::ScaleToRoundedSize(secondary_size, 2.25f);
  gfx::Insets secondary_insets = wayland::GetWorkAreaInsetsInClientPixel(
      secondary_display, 2.25f, secondary_size_in_client_pixel,
      secondary_display.work_area());
  EXPECT_EQ(gfx::Insets(0, 0, 126, 0), secondary_insets);
}

}  // namespace exo
