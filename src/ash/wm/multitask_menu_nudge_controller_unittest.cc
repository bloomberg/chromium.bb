// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/multitask_menu_nudge_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/display/display_move_window_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chromeos/ui/wm/features.h"

namespace ash {

class MultitaskMenuNudgeControllerTest : public AshTestBase {
 public:
  MultitaskMenuNudgeControllerTest() = default;
  MultitaskMenuNudgeControllerTest(const MultitaskMenuNudgeControllerTest&) =
      delete;
  MultitaskMenuNudgeControllerTest& operator=(
      const MultitaskMenuNudgeControllerTest&) = delete;
  ~MultitaskMenuNudgeControllerTest() override = default;

  views::Widget* GetWidget() { return controller_->nudge_widget_.get(); }

  void FireDismissNudgeTimer() { controller_->nudge_dismiss_timer_.FireNow(); }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::wm::features::kFloatWindow);

    AshTestBase::SetUp();

    controller_ = Shell::Get()->multitask_menu_nudge_controller();
    controller_->SetOverrideClockForTesting(&test_clock_);

    // Advance the test clock so we aren't at zero time.
    test_clock_.Advance(base::Hours(50));
  }

  void TearDown() override {
    controller_->SetOverrideClockForTesting(nullptr);

    AshTestBase::TearDown();
  }

 protected:
  base::SimpleTestClock test_clock_;

 private:
  MultitaskMenuNudgeController* controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the nudge is shown after resizing a window.
TEST_F(MultitaskMenuNudgeControllerTest, NudgeShownAfterWindowResize) {
  auto window = CreateAppWindow(gfx::Rect(300, 300));

  // Drag to resize from the bottom right corner of `window`.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point(300, 300));
  event_generator->PressLeftButton();
  EXPECT_FALSE(GetWidget());

  event_generator->MoveMouseBy(10, 10);
  EXPECT_TRUE(GetWidget());
}

TEST_F(MultitaskMenuNudgeControllerTest, NudgeShownAfterStateChange) {
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  ASSERT_FALSE(GetWidget());

  WindowState::Get(window.get())->Maximize();
  EXPECT_TRUE(GetWidget());
}

TEST_F(MultitaskMenuNudgeControllerTest, NudgeTimeout) {
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  WindowState::Get(window.get())->Maximize();
  ASSERT_TRUE(GetWidget());

  FireDismissNudgeTimer();
  EXPECT_FALSE(GetWidget());
}

// Tests that if a window gets destroyed while the nduge is showing, the nudge
// disappears and there is no crash.
TEST_F(MultitaskMenuNudgeControllerTest, WindowDestroyedWhileNudgeShown) {
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  WindowState::Get(window.get())->Maximize();
  ASSERT_TRUE(GetWidget());

  window.reset();
  EXPECT_FALSE(GetWidget());
}

TEST_F(MultitaskMenuNudgeControllerTest, NudgeMultiDisplay) {
  UpdateDisplay("800x700,801+0-800x700");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  auto window = CreateAppWindow(gfx::Rect(300, 300));

  // Maximize and restore so the nudge shows and we can still drag the window.
  WindowState::Get(window.get())->Maximize();
  WindowState::Get(window.get())->Restore();
  ASSERT_TRUE(GetWidget());

  // Drag from the caption the window to the other display. The nudge should be
  // on the other display, even though the window is not (the window stays
  // offscreen and a mirrored version called the drag window is the one on the
  // secondary display).
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point(150, 10));
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(900, 0));
  EXPECT_EQ(Shell::GetAllRootWindows()[1],
            GetWidget()->GetNativeWindow()->GetRootWindow());

  event_generator->ReleaseLeftButton();
  EXPECT_EQ(Shell::GetAllRootWindows()[1],
            GetWidget()->GetNativeWindow()->GetRootWindow());

  display_move_window_util::HandleMoveActiveWindowBetweenDisplays();
  EXPECT_EQ(Shell::GetAllRootWindows()[0],
            GetWidget()->GetNativeWindow()->GetRootWindow());
}

// Tests that based on preferences (shown count, and last shown time), the nudge
// may or may not be shown.
TEST_F(MultitaskMenuNudgeControllerTest, NudgePreferences) {
  // Maximize the window to show the nudge for the first time.
  auto window = CreateAppWindow(gfx::Rect(300, 300));
  WindowState::Get(window.get())->Maximize();
  ASSERT_TRUE(GetWidget());
  FireDismissNudgeTimer();
  ASSERT_FALSE(GetWidget());

  // Restore the window. This does not show the nudge as 24 hours have not
  // elapsed since the nudge was shown.
  WindowState::Get(window.get())->Restore();
  ASSERT_FALSE(GetWidget());

  // Maximize and try restoring again after waiting 25 hours. The nudge should
  // now show for the second time.
  WindowState::Get(window.get())->Maximize();
  test_clock_.Advance(base::Hours(25));
  WindowState::Get(window.get())->Restore();
  ASSERT_TRUE(GetWidget());
  FireDismissNudgeTimer();
  ASSERT_FALSE(GetWidget());

  // Show the nudge for a third time. This will be the last time it is shown.
  test_clock_.Advance(base::Hours(25));
  WindowState::Get(window.get())->Maximize();
  ASSERT_TRUE(GetWidget());
  FireDismissNudgeTimer();
  ASSERT_FALSE(GetWidget());

  // Advance the clock and attempt to show the nudge for a forth time. Verify
  // that it will not show.
  test_clock_.Advance(base::Hours(25));
  WindowState::Get(window.get())->Restore();
  EXPECT_FALSE(GetWidget());
}

}  // namespace ash
