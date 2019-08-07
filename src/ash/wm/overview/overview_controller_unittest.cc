// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_controller.h"

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/keyboard/public/keyboard_switches.h"
#include "ui/keyboard/test/keyboard_test_util.h"

namespace ash {
namespace {

gfx::Point CalculateDragPoint(const WindowResizer& resizer,
                              int delta_x,
                              int delta_y) {
  gfx::Point location = resizer.GetInitialLocation();
  location.set_x(location.x() + delta_x);
  location.set_y(location.y() + delta_y);
  return location;
}

class TestOverviewObserver : public OverviewObserver {
 public:
  enum AnimationState {
    UNKNOWN,
    COMPLETED,
    CANCELED,
  };

  explicit TestOverviewObserver(bool should_monitor_animation_state)
      : should_monitor_animation_state_(should_monitor_animation_state) {
    Shell::Get()->overview_controller()->AddObserver(this);
  }
  ~TestOverviewObserver() override {
    Shell::Get()->overview_controller()->RemoveObserver(this);
  }

  // OverviewObserver:
  void OnOverviewModeStarting() override {
    UpdateLastAnimationWasSlide(
        Shell::Get()->overview_controller()->overview_session());
  }
  void OnOverviewModeEnding(OverviewSession* overview_session) override {
    UpdateLastAnimationWasSlide(overview_session);
  }
  void OnOverviewModeStartingAnimationComplete(bool canceled) override {
    if (!should_monitor_animation_state_)
      return;

    EXPECT_EQ(UNKNOWN, starting_animation_state_);
    starting_animation_state_ = canceled ? CANCELED : COMPLETED;
    if (run_loop_)
      run_loop_->Quit();
  }
  void OnOverviewModeEndingAnimationComplete(bool canceled) override {
    if (!should_monitor_animation_state_)
      return;

    EXPECT_EQ(UNKNOWN, ending_animation_state_);
    ending_animation_state_ = canceled ? CANCELED : COMPLETED;
    if (run_loop_)
      run_loop_->Quit();
  }

  void Reset() {
    starting_animation_state_ = UNKNOWN;
    ending_animation_state_ = UNKNOWN;
  }

  void WaitForStartingAnimationComplete() {
    while (starting_animation_state_ != COMPLETED) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->RunUntilIdle();
    }
  }

  void WaitForEndingAnimationComplete() {
    while (ending_animation_state_ != COMPLETED) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->RunUntilIdle();
    }
  }

  bool is_ended() const { return ending_animation_state_ != UNKNOWN; }
  bool is_started() const { return starting_animation_state_ != UNKNOWN; }
  AnimationState starting_animation_state() const {
    return starting_animation_state_;
  }
  AnimationState ending_animation_state() const {
    return ending_animation_state_;
  }
  bool last_animation_was_slide() const { return last_animation_was_slide_; }

 private:
  void UpdateLastAnimationWasSlide(OverviewSession* selector) {
    DCHECK(selector);
    last_animation_was_slide_ =
        selector->enter_exit_overview_type() ==
        OverviewSession::EnterExitOverviewType::kWindowsMinimized;
  }

  AnimationState starting_animation_state_ = UNKNOWN;
  AnimationState ending_animation_state_ = UNKNOWN;
  bool last_animation_was_slide_ = false;
  // If false, skips the checks in OnOverviewMode Starting/Ending
  // AnimationComplete.
  bool should_monitor_animation_state_;

  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestOverviewObserver);
};

void WaitForOcclusionStateChange(aura::Window* window) {
  auto current_state = window->occlusion_state();
  while (window->occlusion_state() == current_state)
    base::RunLoop().RunUntilIdle();
}

void WaitForShowAnimation(aura::Window* window) {
  while (window->layer()->opacity() != 1.f)
    base::RunLoop().RunUntilIdle();
}

}  // namespace

using OverviewControllerTest = AshTestBase;

// Tests that press the overview key in keyboard when a window is being dragged
// in clamshell mode should not toggle overview.
TEST_F(OverviewControllerTest,
       PressOverviewKeyDuringWindowDragInClamshellMode) {
  ASSERT_FALSE(TabletModeControllerTestApi().IsTabletModeStarted());
  std::unique_ptr<aura::Window> dragged_window = CreateTestWindow();
  std::unique_ptr<WindowResizer> resizer =
      CreateWindowResizer(dragged_window.get(), gfx::Point(), HTCAPTION,
                          ::wm::WINDOW_MOVE_SOURCE_MOUSE);
  resizer->Drag(CalculateDragPoint(*resizer, 10, 0), 0);
  EXPECT_TRUE(wm::GetWindowState(dragged_window.get())->is_dragged());
  GetEventGenerator()->PressKey(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE);
  EXPECT_FALSE(Shell::Get()->overview_controller()->IsSelecting());
  resizer->CompleteDrag();
}

TEST_F(OverviewControllerTest, AnimationCallbacks) {
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  TestOverviewObserver observer(/*should_monitor_animation_state = */ true);
  // Enter without windows.
  auto* shell = Shell::Get();
  shell->overview_controller()->ToggleOverview();
  EXPECT_TRUE(shell->overview_controller()->IsSelecting());
  EXPECT_EQ(TestOverviewObserver::COMPLETED,
            observer.starting_animation_state());
  auto* overview_controller = shell->overview_controller();
  EXPECT_TRUE(overview_controller->HasBlurForTest());
  EXPECT_TRUE(overview_controller->HasBlurAnimationForTest());

  // Exit without windows still creates an animation.
  shell->overview_controller()->ToggleOverview();
  EXPECT_FALSE(shell->overview_controller()->IsSelecting());
  EXPECT_EQ(TestOverviewObserver::UNKNOWN, observer.ending_animation_state());
  EXPECT_TRUE(overview_controller->HasBlurForTest());
  EXPECT_TRUE(overview_controller->HasBlurAnimationForTest());

  observer.WaitForEndingAnimationComplete();
  EXPECT_EQ(TestOverviewObserver::COMPLETED, observer.ending_animation_state());
  EXPECT_FALSE(overview_controller->HasBlurForTest());
  EXPECT_FALSE(overview_controller->HasBlurAnimationForTest());

  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(bounds));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(bounds));

  observer.Reset();
  ASSERT_EQ(TestOverviewObserver::UNKNOWN, observer.starting_animation_state());
  ASSERT_EQ(TestOverviewObserver::UNKNOWN, observer.ending_animation_state());

  // Enter with windows.
  shell->overview_controller()->ToggleOverview();
  EXPECT_TRUE(shell->overview_controller()->IsSelecting());
  EXPECT_EQ(TestOverviewObserver::UNKNOWN, observer.starting_animation_state());
  EXPECT_EQ(TestOverviewObserver::UNKNOWN, observer.ending_animation_state());
  EXPECT_FALSE(overview_controller->HasBlurForTest());
  EXPECT_FALSE(overview_controller->HasBlurAnimationForTest());

  // Exit with windows before starting animation ends.
  shell->overview_controller()->ToggleOverview();
  EXPECT_FALSE(shell->overview_controller()->IsSelecting());
  EXPECT_EQ(TestOverviewObserver::CANCELED,
            observer.starting_animation_state());
  EXPECT_EQ(TestOverviewObserver::UNKNOWN, observer.ending_animation_state());
  // Blur animation never started.
  EXPECT_FALSE(overview_controller->HasBlurForTest());
  EXPECT_FALSE(overview_controller->HasBlurAnimationForTest());

  observer.Reset();

  // Enter again before exit animation ends.
  shell->overview_controller()->ToggleOverview();
  EXPECT_TRUE(shell->overview_controller()->IsSelecting());
  EXPECT_EQ(TestOverviewObserver::UNKNOWN, observer.starting_animation_state());
  EXPECT_EQ(TestOverviewObserver::CANCELED, observer.ending_animation_state());
  // Blur animation will start when animation is completed.
  EXPECT_FALSE(overview_controller->HasBlurForTest());
  EXPECT_FALSE(overview_controller->HasBlurAnimationForTest());

  observer.Reset();

  // Activating window while entering animation should cancel the overview.
  wm::ActivateWindow(window1.get());
  EXPECT_FALSE(shell->overview_controller()->IsSelecting());
  EXPECT_EQ(TestOverviewObserver::CANCELED,
            observer.starting_animation_state());
  // Blur animation never started.
  EXPECT_FALSE(overview_controller->HasBlurForTest());
  EXPECT_FALSE(overview_controller->HasBlurAnimationForTest());
}

// Tests the slide animation for overview is never used in clamshell.
TEST_F(OverviewControllerTest, OverviewEnterExitAnimationClamshell) {
  TestOverviewObserver observer(/*should_monitor_animation_state = */ false);

  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));

  Shell::Get()->overview_controller()->ToggleOverview();
  EXPECT_FALSE(observer.last_animation_was_slide());

  Shell::Get()->overview_controller()->ToggleOverview();
  EXPECT_FALSE(observer.last_animation_was_slide());

  // Even with all window minimized, there should not be a slide animation.
  ASSERT_FALSE(Shell::Get()->overview_controller()->IsSelecting());
  wm::GetWindowState(window.get())->Minimize();
  Shell::Get()->overview_controller()->ToggleOverview();
  EXPECT_FALSE(observer.last_animation_was_slide());
}

// Tests the slide animation for overview is used in tablet if all windows
// are minimized, and that if overview is exited from the home launcher all
// windows are minimized.
TEST_F(OverviewControllerTest, OverviewEnterExitAnimationTablet) {
  TestOverviewObserver observer(/*should_monitor_animation_state = */ false);

  // Ensure calls to EnableTabletModeWindowManager complete.
  base::RunLoop().RunUntilIdle();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  base::RunLoop().RunUntilIdle();

  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));

  Shell::Get()->overview_controller()->ToggleOverview();
  EXPECT_FALSE(observer.last_animation_was_slide());

  // Exit to home launcher. Slide animation should be used, and all windows
  // should be minimized.
  Shell::Get()->overview_controller()->ToggleOverview(
      OverviewSession::EnterExitOverviewType::kWindowsMinimized);
  EXPECT_TRUE(observer.last_animation_was_slide());
  ASSERT_FALSE(Shell::Get()->overview_controller()->IsSelecting());
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMinimized());

  // All windows are minimized, so we should use the slide animation.
  Shell::Get()->overview_controller()->ToggleOverview();
  EXPECT_TRUE(observer.last_animation_was_slide());
}

TEST_F(OverviewControllerTest, OcclusionTest) {
  using OcclusionState = aura::Window::OcclusionState;

  Shell::Get()
      ->overview_controller()
      ->set_occlusion_pause_duration_for_end_ms_for_test(100);
  TestOverviewObserver observer(/*should_monitor_animation_state = */ true);
  ui::ScopedAnimationDurationScaleMode non_zero(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(bounds));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(bounds));
  // Wait for show/hide animation because occlusion tracker because
  // the test depends on opacity.
  WaitForShowAnimation(window1.get());
  WaitForShowAnimation(window2.get());

  window1->TrackOcclusionState();
  window2->TrackOcclusionState();
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());

  // Enter with windows.
  Shell::Get()->overview_controller()->ToggleOverview();
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());

  observer.WaitForStartingAnimationComplete();
  // Occlusion tracking is paused.
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());
  WaitForOcclusionStateChange(window1.get());
  EXPECT_EQ(OcclusionState::VISIBLE, window1->occlusion_state());

  // Exit with windows.
  Shell::Get()->overview_controller()->ToggleOverview();
  EXPECT_EQ(OcclusionState::VISIBLE, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());
  observer.WaitForEndingAnimationComplete();
  EXPECT_EQ(OcclusionState::VISIBLE, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());
  WaitForOcclusionStateChange(window1.get());
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->occlusion_state());

  observer.Reset();

  // Enter again.
  Shell::Get()->overview_controller()->ToggleOverview();
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());
  auto* active = wm::GetActiveWindow();
  EXPECT_EQ(window2.get(), active);

  observer.WaitForStartingAnimationComplete();

  // Window 1 is still occluded because tracker is paused.
  EXPECT_EQ(OcclusionState::OCCLUDED, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());

  WaitForOcclusionStateChange(window1.get());
  EXPECT_EQ(OcclusionState::VISIBLE, window1->occlusion_state());

  wm::ActivateWindow(window1.get());
  observer.WaitForEndingAnimationComplete();

  // Windows are visible because tracker is paused.
  EXPECT_FALSE(Shell::Get()->overview_controller()->IsSelecting());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->occlusion_state());
  EXPECT_EQ(OcclusionState::VISIBLE, window1->occlusion_state());
  WaitForOcclusionStateChange(window2.get());
  EXPECT_EQ(OcclusionState::VISIBLE, window1->occlusion_state());
  EXPECT_EQ(OcclusionState::OCCLUDED, window2->occlusion_state());
}

// Tests that beginning window selection hides the app list.
TEST_F(OverviewControllerTest, SelectingHidesAppList) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplay().id());
  GetAppListTestHelper()->CheckVisibility(true);

  Shell::Get()->overview_controller()->ToggleOverview();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

class OverviewVirtualKeyboardTest : public OverviewControllerTest {
 protected:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    OverviewControllerTest::SetUp();

    TabletModeControllerTestApi().EnterTabletMode();
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(keyboard::IsKeyboardEnabled());
    keyboard::test::WaitUntilLoaded();

    keyboard_controller()->GetKeyboardWindow()->SetBounds(
        keyboard::KeyboardBoundsFromRootBounds(
            Shell::GetPrimaryRootWindow()->bounds(), 100));
    // Wait for keyboard window to load.
    base::RunLoop().RunUntilIdle();
  }

  keyboard::KeyboardController* keyboard_controller() {
    return keyboard::KeyboardController::Get();
  }
};

TEST_F(OverviewVirtualKeyboardTest, ToggleOverviewModeHidesVirtualKeyboard) {
  keyboard_controller()->ShowKeyboard(false /* locked */);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  Shell::Get()->overview_controller()->ToggleOverview();

  // Timeout failure here if the keyboard does not hide.
  keyboard::WaitUntilHidden();
}

TEST_F(OverviewVirtualKeyboardTest,
       ToggleOverviewModeDoesNotHideLockedVirtualKeyboard) {
  keyboard_controller()->ShowKeyboard(true /* locked */);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  Shell::Get()->overview_controller()->ToggleOverview();
  EXPECT_FALSE(keyboard::IsKeyboardHiding());
}

}  // namespace ash
