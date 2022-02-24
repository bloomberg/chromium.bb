// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state.h"

#include <utility>

#include "ash/constants/app_types.h"
#include "ash/metrics/pip_uma.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

using chromeos::WindowStateType;

namespace ash {
namespace {

class AlwaysMaximizeTestState : public WindowState::State {
 public:
  explicit AlwaysMaximizeTestState(WindowStateType initial_state_type)
      : state_type_(initial_state_type) {}

  AlwaysMaximizeTestState(const AlwaysMaximizeTestState&) = delete;
  AlwaysMaximizeTestState& operator=(const AlwaysMaximizeTestState&) = delete;

  ~AlwaysMaximizeTestState() override = default;

  // WindowState::State overrides:
  void OnWMEvent(WindowState* window_state, const WMEvent* event) override {
    // We don't do anything here.
  }
  WindowStateType GetType() const override { return state_type_; }
  void AttachState(WindowState* window_state,
                   WindowState::State* previous_state) override {
    // We always maximize.
    if (state_type_ != WindowStateType::kMaximized) {
      window_state->Maximize();
      state_type_ = WindowStateType::kMaximized;
    }
  }
  void DetachState(WindowState* window_state) override {}

 private:
  WindowStateType state_type_;
};

using WindowStateTest = AshTestBase;
using Sample = base::HistogramBase::Sample;

// Test that a window gets properly snapped to the display's edges in a
// multi monitor environment.
TEST_F(WindowStateTest, SnapWindowBasic) {
  UpdateDisplay("0+0-500x400, 0+500-600x400");
  const gfx::Rect kPrimaryDisplayWorkAreaBounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  const gfx::Rect kSecondaryDisplayWorkAreaBounds =
      GetSecondaryDisplay().work_area();

  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_left);
  gfx::Rect expected = gfx::Rect(kPrimaryDisplayWorkAreaBounds.x(),
                                 kPrimaryDisplayWorkAreaBounds.y(),
                                 kPrimaryDisplayWorkAreaBounds.width() / 2,
                                 kPrimaryDisplayWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  const WMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_right);
  expected.set_x(kPrimaryDisplayWorkAreaBounds.right() - expected.width());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // Move the window to the secondary display.
  window->SetBoundsInScreen(gfx::Rect(600, 0, 100, 100), GetSecondaryDisplay());

  window_state->OnWMEvent(&snap_right);
  expected = gfx::Rect(kSecondaryDisplayWorkAreaBounds.x() +
                           kSecondaryDisplayWorkAreaBounds.width() / 2,
                       kSecondaryDisplayWorkAreaBounds.y(),
                       kSecondaryDisplayWorkAreaBounds.width() / 2,
                       kSecondaryDisplayWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  window_state->OnWMEvent(&snap_left);
  expected.set_x(kSecondaryDisplayWorkAreaBounds.x());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());
}

// Test how the minimum width and maximize behavior specified by the
// aura::WindowDelegate affect snapping in landscape display layout.
TEST_F(WindowStateTest, SnapWindowMinimumSizeLandscape) {
  UpdateDisplay("900x600");
  const gfx::Rect kWorkAreaBounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(0, 100, kWorkAreaBounds.width() - 1, 100)));

  // It should be possible to snap a window with a minimum size.
  const int kMinimumWidth = 700;
  delegate.set_minimum_size(gfx::Size(kMinimumWidth, 0));
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->CanSnap());
  const WMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_right);
  // Expect right snap with the minimum width.
  const gfx::Rect expected_right_snap(kWorkAreaBounds.width() - kMinimumWidth,
                                      kWorkAreaBounds.y(), kMinimumWidth,
                                      kWorkAreaBounds.height());
  EXPECT_EQ(expected_right_snap, window->GetBoundsInScreen());

  // It should not be possible to snap a window if not maximizable.
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      window->GetProperty(aura::client::kResizeBehaviorKey) ^
                          aura::client::kResizeBehaviorCanMaximize);
  EXPECT_FALSE(window_state->CanSnap());
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      window->GetProperty(aura::client::kResizeBehaviorKey) |
                          aura::client::kResizeBehaviorCanMaximize);
  // It should be possible to snap a window if it can be maximized.
  EXPECT_TRUE(window_state->CanSnap());
}

// Test that a unresizable snappable property allows the window to be snapped.
TEST_F(WindowStateTest, UnresizableWindowSnap) {
  const std::array<bool, 2> orientation_params{false, true};

  for (const auto is_landscape : orientation_params) {
    UpdateDisplay(is_landscape ? "900x600,200x100" : "600x900,100x200");
    auto* const screen = display::Screen::GetScreen();
    ASSERT_EQ(2, screen->GetNumDisplays());

    const display::Display primary_display = screen->GetAllDisplays()[0];
    const display::Display secondary_small_display =
        screen->GetAllDisplays()[1];
    ASSERT_EQ(is_landscape, primary_display.is_landscape());
    ASSERT_EQ(is_landscape, secondary_small_display.is_landscape());

    std::unique_ptr<aura::Window> window(
        CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));

    // Make the window unresizable.
    window->SetProperty(aura::client::kResizeBehaviorKey,
                        aura::client::kResizeBehaviorNone);

    auto* const window_state = WindowState::Get(window.get());
    EXPECT_FALSE(window_state->CanSnap());
    EXPECT_FALSE(window_state->CanSnapOnDisplay(primary_display));
    EXPECT_FALSE(window_state->CanSnapOnDisplay(secondary_small_display));

    auto* const opposite_orientation_size =
        is_landscape ? new gfx::Size(0, 300) : new gfx::Size(300, 0);
    window->SetProperty(kUnresizableSnappedSizeKey, opposite_orientation_size);
    EXPECT_FALSE(window_state->CanSnap());
    EXPECT_FALSE(window_state->CanSnapOnDisplay(primary_display));
    EXPECT_FALSE(window_state->CanSnapOnDisplay(secondary_small_display));

    auto* const correct_orientation_size =
        is_landscape ? new gfx::Size(300, 0) : new gfx::Size(0, 300);
    window->SetProperty(kUnresizableSnappedSizeKey, correct_orientation_size);
    EXPECT_TRUE(window_state->CanSnap());
    EXPECT_TRUE(window_state->CanSnapOnDisplay(primary_display));
    EXPECT_FALSE(window_state->CanSnapOnDisplay(secondary_small_display));

    window_util::MoveWindowToDisplay(window.get(),
                                     secondary_small_display.id());
    EXPECT_FALSE(window_state->CanSnap());
    EXPECT_TRUE(window_state->CanSnapOnDisplay(primary_display));
    EXPECT_FALSE(window_state->CanSnapOnDisplay(secondary_small_display));
  }
}

// Test that a unresizable snappable property doesn't have any effect in tablet
// mode.
TEST_F(WindowStateTest, UnresizableWindowSnapInTablet) {
  UpdateDisplay("900x600");

  // Enter tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));

  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorNone);
  window->SetProperty(kUnresizableSnappedSizeKey, new gfx::Size(300, 0));

  auto* const window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->CanSnap());
}

// Test that a window's state type can be changed to PIP via a WM transition
// event.
TEST_F(WindowStateTest, CanTransitionToPipWindow) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));

  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsPip());

  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  EXPECT_TRUE(window_state->IsPip());
}

// Test that a PIP window cannot be snapped.
TEST_F(WindowStateTest, PipWindowCannotSnap) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));

  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->CanSnap());

  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);

  EXPECT_FALSE(window_state->CanSnap());
}

TEST_F(WindowStateTest, ChromePipWindowUmaMetrics) {
  base::HistogramTester histograms;
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));

  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);

  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::PIP_START)));
  EXPECT_EQ(1,
            histograms.GetBucketCount(kAshPipEventsHistogramName,
                                      Sample(AshPipEvents::CHROME_PIP_START)));
  histograms.ExpectTotalCount(kAshPipEventsHistogramName, 2);

  const WMEvent enter_normal(WM_EVENT_NORMAL);
  window_state->OnWMEvent(&enter_normal);

  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::PIP_END)));
  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::CHROME_PIP_END)));
  histograms.ExpectTotalCount(kAshPipEventsHistogramName, 4);
}

TEST_F(WindowStateTest, AndroidPipWindowUmaMetrics) {
  base::HistogramTester histograms;
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::ARC_APP));

  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);

  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::PIP_START)));
  EXPECT_EQ(1,
            histograms.GetBucketCount(kAshPipEventsHistogramName,
                                      Sample(AshPipEvents::ANDROID_PIP_START)));
  histograms.ExpectTotalCount(kAshPipEventsHistogramName, 2);

  const WMEvent enter_normal(WM_EVENT_NORMAL);
  window_state->OnWMEvent(&enter_normal);

  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::PIP_END)));
  EXPECT_EQ(1,
            histograms.GetBucketCount(kAshPipEventsHistogramName,
                                      Sample(AshPipEvents::ANDROID_PIP_END)));
  histograms.ExpectTotalCount(kAshPipEventsHistogramName, 4);

  // Check time count:
  histograms.ExpectTotalCount(kAshPipAndroidPipUseTimeHistogramName, 1);
}

TEST_F(WindowStateTest, ChromePipWindowUmaMetricsCountsExitOnDestroy) {
  base::HistogramTester histograms;
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));

  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);

  // Destroy the window.
  window.reset();

  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::PIP_END)));
  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::CHROME_PIP_END)));
  histograms.ExpectTotalCount(kAshPipEventsHistogramName, 4);
}

TEST_F(WindowStateTest, AndroidPipWindowUmaMetricsCountsExitOnDestroy) {
  base::HistogramTester histograms;
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::ARC_APP));

  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);

  // Destroy the window.
  window.reset();

  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::PIP_END)));
  EXPECT_EQ(1,
            histograms.GetBucketCount(kAshPipEventsHistogramName,
                                      Sample(AshPipEvents::ANDROID_PIP_END)));
  histograms.ExpectTotalCount(kAshPipEventsHistogramName, 4);
}

// Test that modal window dialogs can be snapped.
TEST_F(WindowStateTest, SnapModalWindow) {
  UpdateDisplay("0+0-600x900");
  const gfx::Rect kWorkAreaBounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  aura::test::TestWindowDelegate parent_delegate;
  std::unique_ptr<aura::Window> parent_window(
      CreateTestWindowInShellWithDelegate(
          &parent_delegate, -1,
          gfx::Rect(kWorkAreaBounds.width(), 0, kWorkAreaBounds.width() / 2,
                    kWorkAreaBounds.height() - 1)));

  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(100, 100, 400, 500)));

  delegate.set_minimum_size(gfx::Size(200, 300));

  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->CanSnap());

  ::wm::AddTransientChild(parent_window.get(), window.get());
  EXPECT_TRUE(window_state->CanSnap());

  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanResize);
  EXPECT_FALSE(window_state->CanSnap());

  ::wm::RemoveTransientChild(parent_window.get(), window.get());
}

// Test that the minimum size specified by aura::WindowDelegate gets respected.
TEST_F(WindowStateTest, TestRespectMinimumSize) {
  UpdateDisplay("0+0-1024x768");

  aura::test::TestWindowDelegate delegate;
  const gfx::Size minimum_size(gfx::Size(500, 300));
  delegate.set_minimum_size(minimum_size);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(0, 100, 100, 100)));

  // Check that the window has the correct minimum size.
  EXPECT_EQ(minimum_size.ToString(), window->bounds().size().ToString());

  // Set the size to something bigger - that should work.
  gfx::Rect bigger_bounds(700, 500, 700, 500);
  window->SetBounds(bigger_bounds);
  EXPECT_EQ(bigger_bounds.ToString(), window->bounds().ToString());

  // Set the size to something smaller - that should only resize to the smallest
  // possible size.
  gfx::Rect smaller_bounds(700, 500, 100, 100);
  window->SetBounds(smaller_bounds);
  EXPECT_EQ(minimum_size.ToString(), window->bounds().size().ToString());
}

// Test that the minimum window size specified by aura::WindowDelegate does not
// exceed the screen size.
TEST_F(WindowStateTest, TestIgnoreTooBigMinimumSize) {
  UpdateDisplay("0+0-1024x768");
  const gfx::Size work_area_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area().size();
  const gfx::Size illegal_size(1280, 960);
  const gfx::Rect illegal_bounds(gfx::Point(0, 0), illegal_size);

  aura::test::TestWindowDelegate delegate;
  const gfx::Size minimum_size(illegal_size);
  delegate.set_minimum_size(minimum_size);

  // The creation should force the window to respect the screen size.
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithDelegate(&delegate, -1, illegal_bounds));
  EXPECT_EQ(work_area_size.ToString(), window->bounds().size().ToString());

  // Trying to set the size to something bigger then the screen size should be
  // ignored.
  window->SetBounds(illegal_bounds);
  EXPECT_EQ(work_area_size.ToString(), window->bounds().size().ToString());

  // Maximizing the window should not allow it to go bigger than that either.
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Maximize();
  EXPECT_EQ(work_area_size.ToString(), window->bounds().size().ToString());
}

// Tests UpdateSnapRatio. (1) It should have ratio reset when window
// enters snapped state; (2) it should update ratio on bounds event when
// snapped.
TEST_F(WindowStateTest, UpdateSnapWidthRatioTest) {
  UpdateDisplay("0+0-900x600");
  const gfx::Rect kWorkAreaBounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(100, 100, 100, 100)));
  delegate.set_window_component(HTRIGHT);
  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent cycle_snap_left(WM_EVENT_CYCLE_SNAP_PRIMARY);
  window_state->OnWMEvent(&cycle_snap_left);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  gfx::Rect expected =
      gfx::Rect(kWorkAreaBounds.x(), kWorkAreaBounds.y(),
                kWorkAreaBounds.width() / 2, kWorkAreaBounds.height());
  EXPECT_EQ(expected, window->GetBoundsInScreen());
  EXPECT_EQ(0.5f, *window_state->snap_ratio());

  // Drag to change snapped window width.
  const int kIncreasedWidth = 225;
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(window->bounds().right(), window->bounds().y());
  generator->PressLeftButton();
  generator->MoveMouseTo(window->bounds().right() + kIncreasedWidth,
                         window->bounds().y());
  generator->ReleaseLeftButton();
  expected.set_width(expected.width() + kIncreasedWidth);
  EXPECT_EQ(expected, window->GetBoundsInScreen());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  EXPECT_EQ(0.75f, *window_state->snap_ratio());

  // Another cycle snap left event will restore window state to normal.
  window_state->OnWMEvent(&cycle_snap_left);
  EXPECT_EQ(WindowStateType::kNormal, window_state->GetStateType());
  EXPECT_FALSE(window_state->snap_ratio());

  // Another cycle snap left event will snap window and reset snapped width
  // ratio.
  window_state->OnWMEvent(&cycle_snap_left);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  EXPECT_EQ(0.5f, *window_state->snap_ratio());
}

// Tests that dragging and snapping the snapped window update the width ratio
// correctly (crbug.com/1208969).
TEST_F(WindowStateTest, SnapSnappedWindow) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  UpdateDisplay("800x600");
  const gfx::Rect kWorkAreaBounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  aura::test::TestWindowDelegate delegate;
  gfx::Size window_normal_size = gfx::Size(800, 100);
  std::unique_ptr<aura::Window> window =
      TestWindowBuilder()
          .SetBounds(gfx::Rect(window_normal_size))
          .SetDelegate(&delegate)
          .AllowAllWindowStates()
          .Build();
  delegate.set_window_component(HTCAPTION);
  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent cycle_snap_primary(WM_EVENT_CYCLE_SNAP_PRIMARY);
  window_state->OnWMEvent(&cycle_snap_primary);

  // Snap window to primary position (left).
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  gfx::Rect expected =
      gfx::Rect(kWorkAreaBounds.x(), kWorkAreaBounds.y(),
                kWorkAreaBounds.width() / 2, kWorkAreaBounds.height());
  // Wait for the snapped animation to complete and test that the window bound
  // is primary-snapped and the snap width ratio is updated.
  window->layer()->GetAnimator()->Step(base::TimeTicks::Now() +
                                       base::Seconds(1));
  EXPECT_EQ(expected, window->GetBoundsInScreen());
  EXPECT_EQ(0.5f, *window_state->snap_ratio());

  // Drag the window to unsnap but do not release.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(window->bounds().CenterPoint());
  generator->PressLeftButton();
  generator->MoveMouseBy(5, 0);
  // While dragged, the window size should restore to its normal bound.
  EXPECT_EQ(window_normal_size, window->bounds().size());
  EXPECT_EQ(1.0f, *window_state->snap_ratio());

  // Continue dragging the window and snap it back to the same position.
  generator->MoveMouseBy(-405, 0);
  generator->ReleaseLeftButton();

  // The snapped ratio should be correct regardless of whether the animation
  // is finished or not.
  EXPECT_EQ(0.5f, *window_state->snap_ratio());
}

// Test that snapping left/right preserves the restore bounds.
TEST_F(WindowStateTest, RestoreBounds) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  WindowState* window_state = WindowState::Get(window.get());

  EXPECT_TRUE(window_state->IsNormalStateType());

  // 1) Start with restored window with restore bounds set.
  gfx::Rect restore_bounds = window->GetBoundsInScreen();
  restore_bounds.set_width(restore_bounds.width() + 1);
  window_state->SetRestoreBoundsInScreen(restore_bounds);
  const WMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_left);
  const WMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_right);
  EXPECT_NE(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());
  EXPECT_EQ(restore_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());
  window_state->Restore();
  EXPECT_EQ(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());

  // 2) Start with restored bounds set as a result of maximizing the window.
  window_state->Maximize();
  gfx::Rect maximized_bounds = window->GetBoundsInScreen();
  EXPECT_NE(maximized_bounds.ToString(), restore_bounds.ToString());
  EXPECT_EQ(restore_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());

  window_state->OnWMEvent(&snap_left);
  EXPECT_NE(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());
  EXPECT_NE(maximized_bounds.ToString(),
            window->GetBoundsInScreen().ToString());
  EXPECT_EQ(restore_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

// Test that maximizing an auto managed window, then snapping it puts the window
// at the snapped bounds and not at the auto-managed (centered) bounds.
TEST_F(WindowStateTest, AutoManaged) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  window_state->SetWindowPositionManaged(true);
  window->Hide();
  window->SetBounds(gfx::Rect(100, 100, 100, 100));
  window->Show();

  window_state->Maximize();
  const WMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_right);

  const gfx::Rect kWorkAreaBounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect expected_snapped_bounds(
      kWorkAreaBounds.x() + kWorkAreaBounds.width() / 2, kWorkAreaBounds.y(),
      kWorkAreaBounds.width() / 2, kWorkAreaBounds.height());
  EXPECT_EQ(expected_snapped_bounds.ToString(),
            window->GetBoundsInScreen().ToString());

  // The window should still be auto managed despite being right maximized.
  EXPECT_TRUE(window_state->GetWindowPositionManaged());
}

// Test that the replacement of a State object works as expected.
TEST_F(WindowStateTest, SimpleStateSwap) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  window_state->SetStateObject(std::unique_ptr<WindowState::State>(
      new AlwaysMaximizeTestState(window_state->GetStateType())));
  EXPECT_TRUE(window_state->IsMaximized());
}

// Test that the replacement of a state object, following a restore with the
// original one restores the window to its original state.
TEST_F(WindowStateTest, StateSwapRestore) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  std::unique_ptr<WindowState::State> old(
      window_state->SetStateObject(std::unique_ptr<WindowState::State>(
          new AlwaysMaximizeTestState(window_state->GetStateType()))));
  EXPECT_TRUE(window_state->IsMaximized());
  window_state->SetStateObject(std::move(old));
  EXPECT_FALSE(window_state->IsMaximized());
}

// Tests that a window that had same bounds as the work area shrinks after the
// window is maximized and then restored.
TEST_F(WindowStateTest, RestoredWindowBoundsShrink) {
  UpdateDisplay("0+0-600x900");
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  window->SetBounds(work_area);
  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(work_area.ToString(), window->bounds().ToString());

  window_state->Restore();
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_NE(work_area.ToString(), window->bounds().ToString());
  EXPECT_TRUE(work_area.Contains(window->bounds()));
}

TEST_F(WindowStateTest, DoNotResizeMaximizedWindowInFullscreen) {
  const int shelf_inset_first = 600 - ShelfConfig::Get()->shelf_size();
  const int shelf_inset_second = 700 - ShelfConfig::Get()->shelf_size();
  std::unique_ptr<aura::Window> maximized(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> fullscreen(CreateTestWindowInShellWithId(1));
  WindowState* maximized_state = WindowState::Get(maximized.get());
  maximized_state->Maximize();
  ASSERT_TRUE(maximized_state->IsMaximized());
  EXPECT_EQ(gfx::Rect(0, 0, 800, shelf_inset_first).ToString(),
            maximized->GetBoundsInScreen().ToString());

  // Entering fullscreen mode will not update the maximized window's size
  // under fullscreen.
  WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
  WindowState* fullscreen_state = WindowState::Get(fullscreen.get());
  fullscreen_state->OnWMEvent(&fullscreen_event);
  ASSERT_TRUE(fullscreen_state->IsFullscreen());
  ASSERT_TRUE(maximized_state->IsMaximized());
  EXPECT_EQ(gfx::Rect(0, 0, 800, shelf_inset_first).ToString(),
            maximized->GetBoundsInScreen().ToString());

  // Updating display size will update the maximum window size.
  UpdateDisplay("900x700");
  EXPECT_EQ("0,0 900x700", maximized->GetBoundsInScreen().ToString());
  fullscreen.reset();

  // Exiting fullscreen will update the maximized window to the work area.
  EXPECT_EQ(gfx::Rect(0, 0, 900, shelf_inset_second).ToString(),
            maximized->GetBoundsInScreen().ToString());
}

TEST_F(WindowStateTest, TrustedPinned) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsTrustedPinned());
  window_util::PinWindow(window.get(), true /* trusted */);
  EXPECT_TRUE(window_state->IsTrustedPinned());

  gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  EXPECT_EQ(work_area.ToString(), window->bounds().ToString());

  // Sending non-unpin/non-workspace related event should be ignored.
  {
    const WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
    window_state->OnWMEvent(&fullscreen_event);
  }
  EXPECT_TRUE(window_state->IsTrustedPinned());

  // Update display triggers workspace event.
  UpdateDisplay("300x200");
  EXPECT_EQ("0,0 300x200", window->GetBoundsInScreen().ToString());

  // Unpin should work.
  window_state->Restore();
  EXPECT_FALSE(window_state->IsTrustedPinned());
}

TEST_F(WindowStateTest, AllowSetBoundsDirect) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect original_bounds(50, 50, 200, 200);
  window->SetBounds(original_bounds);
  ASSERT_EQ(original_bounds, window->bounds());

  window_state->set_allow_set_bounds_direct(true);
  window_state->Maximize();

  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(work_area, window->bounds());

  gfx::Rect new_bounds(10, 10, 300, 300);
  window->SetBounds(new_bounds);
  EXPECT_EQ(new_bounds, window->bounds());

  window_state->Restore();
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(original_bounds, window->bounds());

  window_state->set_allow_set_bounds_direct(false);
  window_state->Maximize();

  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(work_area, window->bounds());
  window->SetBounds(new_bounds);
  EXPECT_EQ(work_area, window->bounds());
}

TEST_F(WindowStateTest, FullscreenMinimizedSwitching) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());

  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsFullscreen());

  // Toggling the fullscreen window should restore to normal.
  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsNormalStateType());

  window_state->Maximize();
  ASSERT_TRUE(window_state->IsMaximized());

  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsFullscreen());

  // Toggling the fullscreen window should restore to maximized.
  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsMaximized());

  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsFullscreen());

  // Minimize from fullscreen.
  window_state->Minimize();
  ASSERT_TRUE(window_state->IsMinimized());

  // Unminimize should restore to fullscreen.
  window_state->Unminimize();
  ASSERT_TRUE(window_state->IsFullscreen());

  // Toggling the fullscreen window should restore to maximized.
  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsMaximized());

  // Minimize from fullscreen.
  window_state->Minimize();
  ASSERT_TRUE(window_state->IsMinimized());

  // Fullscreen a minimized window.
  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsFullscreen());

  // Toggling the fullscreen window should not return to minimized. It should
  // return to the state before minimizing and fullscreen.
  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsMaximized());
}

TEST_F(WindowStateTest, CanConsumeSystemKeys) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  WindowState* window_state = WindowState::Get(window.get());

  EXPECT_FALSE(window_state->CanConsumeSystemKeys());

  window->SetProperty(kCanConsumeSystemKeysKey, true);
  EXPECT_TRUE(window_state->CanConsumeSystemKeys());
}

TEST_F(WindowStateTest,
       RestoreStateAfterEnteringPipViaOcculusionAndDismissingPip) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  window->Show();
  EXPECT_TRUE(window->layer()->visible());

  // Ensure a maximized window gets maximized again after it enters PIP via
  // occlusion, gets minimized, and unminimized.
  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());

  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  EXPECT_TRUE(window_state->IsPip());

  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  window_state->Unminimize();
  EXPECT_TRUE(window_state->IsMaximized());

  // Ensure a freeform window gets freeform again after it enters PIP via
  // occulusion, gets minimized, and unminimized.
  ::wm::SetWindowState(window.get(), ui::SHOW_STATE_NORMAL);

  window_state->OnWMEvent(&enter_pip);
  EXPECT_TRUE(window_state->IsPip());

  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  window_state->Unminimize();
  EXPECT_TRUE(window_state->GetStateType() == WindowStateType::kNormal);
}

TEST_F(WindowStateTest, RestoreStateAfterEnterPipViaMinimizeAndDismissingPip) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  window->Show();
  EXPECT_TRUE(window->layer()->visible());

  // Ensure a maximized window gets maximized again after it enters PIP via
  // minimize, gets minimized, and unminimized.
  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());

  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  EXPECT_TRUE(window_state->IsPip());

  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  window_state->Unminimize();
  EXPECT_TRUE(window_state->IsMaximized());

  // Ensure a freeform window gets freeform again after it enters PIP via
  // minimize, gets minimized, and unminimized.
  ::wm::SetWindowState(window.get(), ui::SHOW_STATE_NORMAL);

  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  window_state->OnWMEvent(&enter_pip);
  EXPECT_TRUE(window_state->IsPip());

  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  window_state->Unminimize();
  EXPECT_TRUE(window_state->GetStateType() == WindowStateType::kNormal);
}

TEST_F(WindowStateTest, SetBoundsUpdatesSizeOfPipRestoreBounds) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  window->Show();
  window->SetBounds(gfx::Rect(0, 0, 50, 50));

  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);

  EXPECT_TRUE(window_state->IsPip());
  EXPECT_TRUE(window_state->HasRestoreBounds());
  EXPECT_EQ(gfx::Rect(8, 8, 50, 50), window_state->GetRestoreBoundsInScreen());
  window_state->window()->SetBounds(gfx::Rect(100, 100, 100, 100));
  // SetBounds only updates the size of the restore bounds.
  EXPECT_EQ(gfx::Rect(8, 8, 100, 100),
            window_state->GetRestoreBoundsInScreen());
}

TEST_F(WindowStateTest, SetBoundsSnapsPipBoundsToScreenEdge) {
  UpdateDisplay("600x900");

  aura::test::TestWindowDelegate delegate;
  delegate.set_minimum_size(gfx::Size(51, 51));
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(0, 0, 50, 50)));
  WindowState* window_state = WindowState::Get(window.get());
  window->Show();

  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  window->SetBounds(gfx::Rect(542, 50, 50, 50));
  EXPECT_TRUE(window_state->IsPip());
  // Ensure that the PIP window is along the right edge of the screen even when
  // the new bounds is adjusted by the minimum size.
  // 541 (left origin) + 51 (PIP width) + 8 (PIP insets) == 600.
  EXPECT_EQ(gfx::Rect(541, 50, 51, 51),
            window_state->window()->GetBoundsInScreen());

  PipPositioner::SaveSnapFraction(window_state,
                                  window_state->window()->GetBoundsInScreen());
  EXPECT_TRUE(PipPositioner::HasSnapFraction(window_state));
  EXPECT_EQ(gfx::Rect(541, 50, 51, 51),
            PipPositioner::GetPositionAfterMovementAreaChange(window_state));
}

// Make sure the window is transparent only when it is in normal state.
TEST_F(WindowStateTest, OpacityChange) {
  std::unique_ptr<aura::Window> window = CreateAppWindow();
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(window->GetTransparent());

  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_FALSE(window->GetTransparent());

  window_state->Restore();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(window->GetTransparent());

  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());
  EXPECT_FALSE(window->GetTransparent());

  window_state->Unminimize();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(window->GetTransparent());

  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsFullscreen());
  EXPECT_FALSE(window->GetTransparent());

  window_state->Restore();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(window->GetTransparent());

  const WMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_left);
  EXPECT_FALSE(window->GetTransparent());

  window_state->Restore();
  EXPECT_TRUE(window->GetTransparent());

  const WMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_left);
  EXPECT_FALSE(window->GetTransparent());

  window_state->OnWMEvent(&snap_left);
  EXPECT_FALSE(window->GetTransparent());
}

// Tests the basic functionalties related to window state restore history stack.
TEST_F(WindowStateTest, WindowStateRestoreHistoryBasicFunctionalites) {
  // Start with kDefault window state.
  std::unique_ptr<aura::Window> window = CreateAppWindow();
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsNormalStateType());

  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history_for_testing();
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);

  // Transition to kPrimarySnapped window state.
  const WMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_left);
  EXPECT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);

  // Then transition to kMaximized window state.
  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);
  EXPECT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kPrimarySnapped);
  EXPECT_EQ(window_state->GetRestoreWindowState(),
            WindowStateType::kPrimarySnapped);

  // Then transition to kFullscreen window state.
  const WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
  window_state->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(restore_stack.size(), 3u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kPrimarySnapped);
  EXPECT_EQ(restore_stack[2], WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kMaximized);

  // Then transition to kMinimized window state.
  const WMEvent minimized_event(WM_EVENT_MINIMIZE);
  window_state->OnWMEvent(&minimized_event);
  EXPECT_EQ(restore_stack.size(), 4u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kPrimarySnapped);
  EXPECT_EQ(restore_stack[2], WindowStateType::kMaximized);
  EXPECT_EQ(restore_stack[3], WindowStateType::kFullscreen);
  EXPECT_EQ(window_state->GetRestoreWindowState(),
            WindowStateType::kFullscreen);

  // Then start restore from here. It should restore back to kFullscreen window
  // state.
  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kFullscreen);
  EXPECT_EQ(restore_stack.size(), 3u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kPrimarySnapped);
  EXPECT_EQ(restore_stack[2], WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kMaximized);

  // Then restore back to kMaximized window state.
  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kMaximized);
  EXPECT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kPrimarySnapped);
  EXPECT_EQ(window_state->GetRestoreWindowState(),
            WindowStateType::kPrimarySnapped);

  // Then restore back to kPrimarySnapped window state.
  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kPrimarySnapped);
  EXPECT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);

  // Then restore back to kNormal window state.
  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);

  // Restore a kNormal window state window will keep the window's kNormal window
  // state.
  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
}

// Tests that window state transitioning from higher to lower layer will erase
// the window state restore history in between.
TEST_F(WindowStateTest, TransitionFromHighToLowerLayerEraseRestoreHistory) {
  // Start with kDefault window state.
  std::unique_ptr<aura::Window> window = CreateAppWindow();
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsNormalStateType());

  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history_for_testing();
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);

  // Transition to kPrimarySnapped window state.
  const WMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_left);

  // Then transition to kMaximized window state.
  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);

  // Then transition to kFullscreen window state.
  const WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
  window_state->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(restore_stack.size(), 3u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kPrimarySnapped);
  EXPECT_EQ(restore_stack[2], WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kMaximized);

  // Now transition back to kPrimarySnapped window state. It should have erased
  // any restore history after kPrimarySnapped.
  window_state->OnWMEvent(&snap_left);
  EXPECT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
}

// Tests the restore behaviors when window state transitions in the same layer.
// There are 3 cases: {kNormal & kDefault}, {kPrimarySnapped &
// kSecondarySnapped}, and {kMinimized & kPip}.
TEST_F(WindowStateTest, TransitionInTheSameLayerKeepSameRestoreHistory) {
  // First we test kNormal & kDefault.
  // Start with kDefault window state.
  std::unique_ptr<aura::Window> window = CreateAppWindow();
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsNormalStateType());

  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history_for_testing();
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);

  // Transition to kNormal window state. Since it's on the same layer as
  // kDefault, kDefault won't be pushed into the restore history stack.
  const WMEvent normal_event(WM_EVENT_NORMAL);
  window_state->OnWMEvent(&normal_event);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);

  // Test kPrimarySnapped & kSecondarySnapped.
  // Transition to kPrimarySnapped window state.
  const WMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_left);
  EXPECT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);

  // Transition to kSecondarySnapped window state. Since it's on the same layer
  // as kPrimarySnapped, kPrimarySnapped won't be pushed into the restore
  // history stack.
  const WMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_right);
  EXPECT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);

  // Test kMinimized & kPip.
  // Transition to kMinimized window state.
  const WMEvent minimized_event(WM_EVENT_MINIMIZE);
  window_state->OnWMEvent(&minimized_event);
  EXPECT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kNormal);
  EXPECT_EQ(restore_stack[1], WindowStateType::kSecondarySnapped);
  EXPECT_EQ(window_state->GetRestoreWindowState(),
            WindowStateType::kSecondarySnapped);

  // Transition to kPip Window state. Since it's on the same layer as
  // kMinimized, kMinimized won't be pushed into the restore history stack.
  const WMEvent pip_event(WM_EVENT_PIP);
  window_state->OnWMEvent(&pip_event);
  EXPECT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kNormal);
  EXPECT_EQ(restore_stack[1], WindowStateType::kSecondarySnapped);
  EXPECT_EQ(window_state->GetRestoreWindowState(),
            WindowStateType::kSecondarySnapped);
}

// Test the restore behaviors of kPinned and kTrustedPinned window state. They
// are different with kFullscreen restore behaviors.
TEST_F(WindowStateTest, PinnedRestoreTest) {
  // Start with kDefault window state.
  std::unique_ptr<aura::Window> window = CreateAppWindow();
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsNormalStateType());

  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history_for_testing();
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);

  // Transition to kPrimarySnapped window state.
  const WMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_left);

  // Then transition to kMaximized window state.
  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);
  EXPECT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kPrimarySnapped);
  EXPECT_EQ(window_state->GetRestoreWindowState(),
            WindowStateType::kPrimarySnapped);

  // Then transition to kPinned window state. Since kPinned window state is not
  // supported in the window state restore history layer, the restore history
  // stack will be cleared. It can only restore back to kNormal window state.
  const WMEvent pinned_event(WM_EVENT_PIN);
  window_state->OnWMEvent(&pinned_event);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);

  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);

  // Same should happen for kTrustedPinned as well.
  window_state->OnWMEvent(&snap_left);
  window_state->OnWMEvent(&maximize_event);
  EXPECT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(window_state->GetRestoreWindowState(),
            WindowStateType::kPrimarySnapped);

  const WMEvent trusted_pinned_event(WM_EVENT_TRUSTED_PIN);
  window_state->OnWMEvent(&trusted_pinned_event);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);

  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
}

// Test the restore behaviors of kMinimized and kPip window state. They are both
// viewed as the final state in the restore layer.
TEST_F(WindowStateTest, MinimizedAndPipRestoreTest) {
  // Start with kDefault window state.
  std::unique_ptr<aura::Window> window = CreateAppWindow();
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsNormalStateType());

  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history_for_testing();
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);

  // Maximize the window.
  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);
  EXPECT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);

  // kPip window can be minimized to kMinimized window state, but restoring from
  // kMinimized window state can't restore back to kPip window state.
  const WMEvent pip_event(WM_EVENT_PIP);
  window_state->OnWMEvent(&pip_event);
  EXPECT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kMaximized);

  const WMEvent minimized_event(WM_EVENT_MINIMIZE);
  window_state->OnWMEvent(&minimized_event);
  EXPECT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kMaximized);

  // Restore the minimized window. It should go back to pre-pip window state.
  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kMaximized);
  EXPECT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);

  // Similarly, if the pre-pip window state is kMinimized, restoring from kPip
  // should go back to the pre-minimized window state.
  window_state->OnWMEvent(&minimized_event);
  EXPECT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kMaximized);

  window_state->OnWMEvent(&pip_event);
  EXPECT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kMaximized);

  // Restore the Pip window. It should go back to pre-minimized window state.
  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kMaximized);
  EXPECT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
}

// Tests the restore behavior for default or normal window.
TEST_F(WindowStateTest, NormalOrDefaultRestore) {
  // Start with kDefault window state.
  std::unique_ptr<aura::Window> window = CreateAppWindow();
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kDefault);

  // Restoring a kDefault window will change its window state to kNormal.
  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);

  // Restoring kNormal window will do nothing.
  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);
}

// Test WindowStateTest functionalities with portrait display. This test is
// parameterized to enable vertical layout or horizontal layout snap in
// portrait display.
class PortraitDisplayWindowStateTest
    : public AshTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  PortraitDisplayWindowStateTest() = default;
  PortraitDisplayWindowStateTest(const PortraitDisplayWindowStateTest&) =
      delete;
  PortraitDisplayWindowStateTest& operator=(
      const PortraitDisplayWindowStateTest&) = delete;
  ~PortraitDisplayWindowStateTest() override = default;

  bool IsVerticalSnapEnabled() const { return GetParam(); }

  // WindowStateTest:
  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          chromeos::wm::features::kVerticalSnap);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          chromeos::wm::features::kVerticalSnap);
    }
    AshTestBase::SetUp();
    UpdateDisplay("600x900");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test how the minimum height specified by the aura::WindowDelegate affects
// snapping in portrait display layout.
TEST_P(PortraitDisplayWindowStateTest, SnapWindowMinimumSizePortrait) {
  const gfx::Rect kWorkAreaBounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(0, 100, kWorkAreaBounds.width() - 1, 100)));

  // It should be possible to snap a window with a minimum width that is larger
  // a half screen width in horizontal snap layout and snap a window with a
  // minimum height that is longer than a half screen height in vertical snap
  // layout.
  const gfx::Size kMinimumSize =
      IsVerticalSnapEnabled() ? gfx::Size(0, 500) : gfx::Size(400, 0);
  delegate.set_minimum_size(kMinimumSize);
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->CanSnap());
  const WMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_right);
  // Expect right snap for horizontal snap layout with the minimum width and
  // bottom snap for vertical snap layout with the minimum height.
  const gfx::Rect expected_snap =
      IsVerticalSnapEnabled()
          ? gfx::Rect(kWorkAreaBounds.x(),
                      kWorkAreaBounds.height() - kMinimumSize.height(),
                      kWorkAreaBounds.width(), kMinimumSize.height())
          : gfx::Rect(kWorkAreaBounds.width() - kMinimumSize.width(),
                      kWorkAreaBounds.y(), kMinimumSize.width(),
                      kWorkAreaBounds.height());
  EXPECT_EQ(expected_snap, window->GetBoundsInScreen());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PortraitDisplayWindowStateTest,
                         ::testing::Bool());

// TODO(skuhne): Add more unit test to verify the correctness for the restore
// operation.

}  // namespace
}  // namespace ash
