// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/persistent_window_controller.h"

#include "ash/display/display_move_window_util.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/display/test/display_manager_test_api.h"

using session_manager::SessionState;

namespace ash {

using PersistentWindowControllerTest = AshTestBase;

TEST_F(PersistentWindowControllerTest, DisconnectDisplay) {
  UpdateDisplay("0+0-500x500,0+501-500x500");

  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // Disconnects secondary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  // Reconnects secondary display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  // Disconnects primary display.
  display_info_list.clear();
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  // Reconnects primary display.
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  // Disconnects secondary display.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // A third id which is different from primary and secondary.
  const int64_t third_id = secondary_id + 1;
  display::ManagedDisplayInfo third_info =
      display::CreateDisplayInfo(third_id, gfx::Rect(0, 501, 500, 500));
  // Connects another secondary display with |third_id|.
  display_info_list.push_back(third_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
  // Connects secondary display with |secondary_id|.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  // Disconnects secondary display.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // Sets |w2|'s bounds changed by user and then reconnects secondary display.
  WindowState* w2_state = WindowState::Get(w2);
  w2_state->set_bounds_changed_by_user(true);
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
}

TEST_F(PersistentWindowControllerTest, ThreeDisplays) {
  UpdateDisplay("0+0-500x500,0+501-500x500,0+1002-500x500");

  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  aura::Window* w3 =
      CreateTestWindowInShellWithBounds(gfx::Rect(1002, 0, 400, 200));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1002, 0, 400, 200), w3->GetBoundsInScreen());

  const int64_t primary_id = display_manager()->GetDisplayAt(0).id();
  const int64_t second_id = display_manager()->GetDisplayAt(1).id();
  const int64_t third_id = display_manager()->GetDisplayAt(2).id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo second_info =
      display_manager()->GetDisplayInfo(second_id);
  display::ManagedDisplayInfo third_info =
      display_manager()->GetDisplayInfo(third_id);

  // Disconnects third display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_info_list.push_back(second_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(2, 0, 400, 200), w3->GetBoundsInScreen());

  // Disconnects second display.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(2, 0, 400, 200), w3->GetBoundsInScreen());

  // Reconnects third display.
  display_info_list.push_back(third_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(502, 0, 400, 200), w3->GetBoundsInScreen());

  // Reconnects second display.
  display_info_list.push_back(second_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1002, 0, 400, 200), w3->GetBoundsInScreen());

  // Disconnects both external displays.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(2, 0, 400, 200), w3->GetBoundsInScreen());

  // Reconnects second display.
  display_info_list.push_back(second_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(2, 0, 400, 200), w3->GetBoundsInScreen());

  // Reconnects third display.
  display_info_list.push_back(third_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1002, 0, 400, 200), w3->GetBoundsInScreen());
}

TEST_F(PersistentWindowControllerTest, NormalMirrorMode) {
  UpdateDisplay("0+0-500x500,0+501-500x500");

  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  // Enables mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, base::nullopt);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
  // Disables mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
}

TEST_F(PersistentWindowControllerTest, MixedMirrorMode) {
  UpdateDisplay("0+0-500x500,0+501-500x500,0+1002-500x500");
  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  aura::Window* w3 =
      CreateTestWindowInShellWithBounds(gfx::Rect(1002, 0, 400, 200));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1002, 0, 400, 200), w3->GetBoundsInScreen());

  const int64_t primary_id = display_manager()->GetDisplayAt(0).id();
  const int64_t second_id = display_manager()->GetDisplayAt(1).id();
  const int64_t third_id = display_manager()->GetDisplayAt(2).id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo second_info =
      display_manager()->GetDisplayInfo(second_id);
  display::ManagedDisplayInfo third_info =
      display_manager()->GetDisplayInfo(third_id);

  // Turn on mixed mirror mode. (Mirror from the primary display to the second
  // display).
  display::DisplayIdList dst_ids;
  dst_ids.emplace_back(second_id);
  display_manager()->SetMirrorMode(
      display::MirrorMode::kMixed,
      base::make_optional<display::MixedMirrorModeParams>(primary_id, dst_ids));
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_TRUE(display_manager()->mixed_mirror_mode_params());
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(502, 0, 400, 200), w3->GetBoundsInScreen());

  // Turn off mixed mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_FALSE(display_manager()->mixed_mirror_mode_params());
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1002, 0, 400, 200), w3->GetBoundsInScreen());
}

TEST_F(PersistentWindowControllerTest, WindowMovedByAccel) {
  UpdateDisplay("0+0-500x500,0+501-500x500");

  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // Disconnects secondary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  // Reconnects secondary display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  // Moves |w2| to primary display by accelerators after we reset the persistent
  // window info. It should be able to save persistent window info again on next
  // display change.
  wm::ActivateWindow(w2);
  display_move_window_util::HandleMoveActiveWindowBetweenDisplays();
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  // Disconnects secondary display.
  display_info_list.clear();
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  // Reconnects secondary display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());
}

TEST_F(PersistentWindowControllerTest, ReconnectOnLockScreen) {
  UpdateDisplay("0+0-500x500,0+501-500x500");

  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // Disconnects secondary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  // Spin a run loop to ensure shelf is deleted. https://crbug.com/810807.
  base::RunLoop().RunUntilIdle();

  // Enters locked session state and reconnects secondary display.
  GetSessionControllerClient()->SetSessionState(SessionState::LOCKED);
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  // Unlocks and checks that |w2| is restored.
  GetSessionControllerClient()->SetSessionState(SessionState::ACTIVE);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());
}

TEST_F(PersistentWindowControllerTest, RecordNumOfWindowsRestored) {
  UpdateDisplay("0+0-500x500,0+501-500x500");
  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // Disconnects secondary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(1, 0, 200, 100), w2->GetBoundsInScreen());

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      PersistentWindowController::kNumOfWindowsRestoredHistogramName, 0);

  // Reconnects secondary display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  histogram_tester.ExpectTotalCount(
      PersistentWindowController::kNumOfWindowsRestoredHistogramName, 1);
}

// Tests that swapping primary display shall not do persistent window restore.
TEST_F(PersistentWindowControllerTest, SwapPrimaryDisplay) {
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  const display::ManagedDisplayInfo native_display_info =
      display::CreateDisplayInfo(internal_display_id,
                                 gfx::Rect(0, 0, 500, 500));
  const display::ManagedDisplayInfo secondary_display_info =
      display::CreateDisplayInfo(10, gfx::Rect(1, 1, 400, 400));

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_info_list.push_back(secondary_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  aura::Window* w1 =
      CreateTestWindowInShellWithBounds(gfx::Rect(200, 0, 100, 200));
  aura::Window* w2 =
      CreateTestWindowInShellWithBounds(gfx::Rect(501, 0, 200, 100));
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(501, 0, 200, 100), w2->GetBoundsInScreen());

  // Swaps primary display and check window bounds.
  SwapPrimaryDisplay();
  ASSERT_EQ(gfx::Rect(-500, 0, 500, 500),
            display_manager()->GetDisplayForId(internal_display_id).bounds());
  ASSERT_EQ(gfx::Rect(0, 0, 400, 400),
            display_manager()->GetDisplayForId(10).bounds());
  EXPECT_EQ(gfx::Rect(200, 0, 100, 200), w1->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(-499, 0, 200, 100), w2->GetBoundsInScreen());
}

// Tests that restore bounds persist after adding and removing a display.
TEST_F(PersistentWindowControllerTest, RestoreBounds) {
  UpdateDisplay("0+0-500x500,0+501-500x500");

  std::unique_ptr<aura::Window> window = CreateTestWindow(gfx::Rect(200, 200));
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();
  display::Screen* screen = display::Screen::GetScreen();
  ASSERT_EQ(primary_id, screen->GetDisplayNearestWindow(window.get()).id());

  // Move the window to the secondary display and maximize it.
  display_move_window_util::HandleMoveActiveWindowBetweenDisplays();
  ASSERT_EQ(secondary_id, screen->GetDisplayNearestWindow(window.get()).id());
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Maximize();
  EXPECT_TRUE(window_state->HasRestoreBounds());
  const gfx::Rect restore_bounds_in_screen =
      window_state->GetRestoreBoundsInScreen();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // Disconnect secondary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(primary_id, screen->GetDisplayNearestWindow(window.get()).id());

  // Reconnect secondary display. On restoring the maximized window, the bounds
  // should be the same as they were before maximizing and disconnecting the
  // display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(window.get()).id());
  EXPECT_TRUE(window_state->IsMaximized());

  // Restore the window (i.e. press restore button on header).
  window_state->Restore();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(restore_bounds_in_screen, window->GetBoundsInScreen());
}

// Tests that the MRU order is maintained visually after adding and removing a
// display.
TEST_F(PersistentWindowControllerTest, MRUOrderMatchesStacking) {
  UpdateDisplay("0+0-500x500,0+501-500x500");

  // Add three windows, all on the secondary display.
  const gfx::Rect bounds(500, 0, 200, 200);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds);

  // MRU order should be opposite of the order the windows were created. Verify
  // that all three windows are indeed on the secondary display.
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();
  display::Screen* screen = display::Screen::GetScreen();
  const std::vector<aura::Window*> expected_mru_order = {
      window3.get(), window2.get(), window1.get()};
  ASSERT_EQ(
      expected_mru_order,
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks));
  for (auto* window : expected_mru_order)
    ASSERT_EQ(secondary_id, screen->GetDisplayNearestWindow(window).id());

  // Disconnect secondary display. The windows should move to the primary
  // display and retain MRU ordering.
  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // The order which the children are stacked in is the reverse of the order
  // they are in the children() field.
  aura::Window* parent = window1->parent();
  ASSERT_TRUE(parent);
  std::vector<aura::Window*> children_ordered_by_stacking = parent->children();
  std::reverse(children_ordered_by_stacking.begin(),
               children_ordered_by_stacking.end());
  EXPECT_EQ(
      expected_mru_order,
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks));
  EXPECT_EQ(expected_mru_order, children_ordered_by_stacking);
  EXPECT_EQ(primary_id, screen->GetDisplayNearestWindow(parent).id());

  // Reconnect secondary display. The windows should move to the secondary
  // display and retain MRU ordering.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  parent = window1->parent();
  children_ordered_by_stacking = parent->children();
  std::reverse(children_ordered_by_stacking.begin(),
               children_ordered_by_stacking.end());
  ASSERT_TRUE(parent);
  EXPECT_EQ(
      expected_mru_order,
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks));
  EXPECT_EQ(expected_mru_order, children_ordered_by_stacking);
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(parent).id());
}

// Similar to the above test but with windows created on both displays.
TEST_F(PersistentWindowControllerTest, MRUOrderMatchesStackingInterleaved) {
  UpdateDisplay("0+0-500x500,0+501-500x500");

  // Add four windows, two on each display.
  const gfx::Rect primary_bounds(200, 200);
  const gfx::Rect secondary_bounds(500, 0, 200, 200);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(primary_bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(secondary_bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(primary_bounds);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(secondary_bounds);

  // MRU order should be opposite of the order the windows were created.
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();
  display::Screen* screen = display::Screen::GetScreen();
  const std::vector<aura::Window*> expected_mru_order = {
      window4.get(), window3.get(), window2.get(), window1.get()};
  ASSERT_EQ(
      expected_mru_order,
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks));

  // Disconnect secondary display. The windows should move to the primary
  // display and retain MRU ordering. Note that this logic is part of
  // RootWindowController and not PersistentWindowController.
  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // The order which the children are stacked in is the reverse of the order
  // they are in the children() field.
  aura::Window* parent = window1->parent();
  ASSERT_TRUE(parent);
  ASSERT_EQ(parent, window2->parent());
  std::vector<aura::Window*> children_ordered_by_stacking = parent->children();
  std::reverse(children_ordered_by_stacking.begin(),
               children_ordered_by_stacking.end());
  EXPECT_EQ(
      expected_mru_order,
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks));
  EXPECT_EQ(expected_mru_order, children_ordered_by_stacking);
  EXPECT_EQ(primary_id, screen->GetDisplayNearestWindow(parent).id());

  // Reconnect secondary display. |window2| and |window4| should move back to
  // the secondary display.
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(
      expected_mru_order,
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kAllDesks));
  parent = window1->parent();
  EXPECT_EQ(primary_id, screen->GetDisplayNearestWindow(parent).id());
  ASSERT_EQ(2u, parent->children().size());
  EXPECT_EQ(window1.get(), parent->children()[0]);
  EXPECT_EQ(window3.get(), parent->children()[1]);

  parent = window2->parent();
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(parent).id());
  ASSERT_EQ(2u, parent->children().size());
  EXPECT_EQ(window2.get(), parent->children()[0]);
  EXPECT_EQ(window4.get(), parent->children()[1]);
}

}  // namespace ash
