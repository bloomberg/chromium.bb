// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_state.h"

#include <stdint.h>

#include "ash/scoped_root_window_for_new_windows.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"

namespace ash {
namespace {

using ShellStateTest = AshTestBase;

TEST_F(ShellStateTest, GetDisplayForNewWindows) {
  UpdateDisplay("1024x768,800x600");
  const int64_t primary_display_id = display_manager()->GetDisplayAt(0).id();
  const int64_t secondary_display_id = display_manager()->GetDisplayAt(1).id();

  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(primary_display_id, screen->GetDisplayForNewWindows().id());

  // Setting a root window for new windows notifies the client.
  ScopedRootWindowForNewWindows scoped_root(Shell::GetAllRootWindows()[1]);
  EXPECT_EQ(secondary_display_id, screen->GetDisplayForNewWindows().id());
}

}  // namespace
}  // namespace ash
