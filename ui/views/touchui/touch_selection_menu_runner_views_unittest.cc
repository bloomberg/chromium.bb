// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/strings/grit/ui_strings.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/touchui/touch_selection_menu_runner_views.h"

namespace views {
namespace {

// Should match |kMenuButtonWidth| in touch_selection_menu_runner_views.cc.
const int kMenuButtonWidth = 63;

// Should match size of |kMenuCommands| array in
// touch_selection_menu_runner_views.cc.
const int kMenuCommandCount = 3;

}

class TouchSelectionMenuRunnerViewsTest : public ViewsTestBase,
                                          public ui::TouchSelectionMenuClient {
 public:
  TouchSelectionMenuRunnerViewsTest() : no_command_available_(false) {}
  ~TouchSelectionMenuRunnerViewsTest() override {}

 protected:
  void set_no_commmand_available(bool no_command) {
    no_command_available_ = no_command;
  }

  gfx::Rect GetMenuAnchorRect() {
    TouchSelectionMenuRunnerViews* menu_runner =
        static_cast<TouchSelectionMenuRunnerViews*>(
            ui::TouchSelectionMenuRunner::GetInstance());
    return menu_runner->GetAnchorRectForTest();
  }

 private:
  // ui::TouchSelectionMenuClient:
  bool IsCommandIdEnabled(int command_id) const override {
    return !no_command_available_;
  }

  void ExecuteCommand(int command_id, int event_flags) override {}

  void RunContextMenu() override {}

  // When set to true, no command would be availble and menu should not be
  // shown.
  bool no_command_available_;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionMenuRunnerViewsTest);
};

TEST_F(TouchSelectionMenuRunnerViewsTest, InstalledAndWorksProperly) {
  gfx::Rect menu_anchor(0, 0, 10, 10);
  gfx::Size handle_size(10, 10);

  // Menu runner instance should be installed, but no menu should be running.
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Run menu. Since commands are availble, this should bring up menus.
  ui::TouchSelectionMenuRunner::GetInstance()->OpenMenu(
      this, menu_anchor, handle_size, GetContext());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Close menu.
  ui::TouchSelectionMenuRunner::GetInstance()->CloseMenu();
  RunPendingMessages();
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Try running menu when no commands is available. Menu should not be shown.
  set_no_commmand_available(true);
  ui::TouchSelectionMenuRunner::GetInstance()->OpenMenu(
      this, menu_anchor, handle_size, GetContext());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

// Tests if anchor rect for the quick menu is adjusted correctly based on the
// distance of handles.
TEST_F(TouchSelectionMenuRunnerViewsTest, QuickMenuAdjustsAnchorRect) {
  gfx::Size handle_size(10, 10);

  // Calculate the width of quick menu. In addition to |kMenuCommandCount|
  // commands, there is an item for ellipsis.
  int quick_menu_width =
      (kMenuCommandCount + 1) * kMenuButtonWidth + kMenuCommandCount;

  // Set anchor rect's width a bit smaller than the quick menu width plus handle
  // image width and check that anchor rect's height is adjusted.
  gfx::Rect anchor_rect(0, 0, quick_menu_width + handle_size.width() - 10, 20);
  ui::TouchSelectionMenuRunner::GetInstance()->OpenMenu(
      this, anchor_rect, handle_size, GetContext());
  anchor_rect.Inset(0, 0, 0, -handle_size.height());
  EXPECT_EQ(anchor_rect, GetMenuAnchorRect());

  // Set anchor rect's width a bit greater than the quick menu width plus handle
  // image width and check that anchor rect's height is not adjusted.
  anchor_rect =
      gfx::Rect(0, 0, quick_menu_width + handle_size.width() + 10, 20);
  ui::TouchSelectionMenuRunner::GetInstance()->OpenMenu(
      this, anchor_rect, handle_size, GetContext());
  EXPECT_EQ(anchor_rect, GetMenuAnchorRect());

  ui::TouchSelectionMenuRunner::GetInstance()->CloseMenu();
  RunPendingMessages();
}

}  // namespace views
