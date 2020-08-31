// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_controller.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/magnifier/docked_magnifier_controller_impl.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/fps_counter.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/multi_display_overview_and_split_view_test.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_delegate.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_drag_delegate.h"
#include "ash/wm/tablet_mode/tablet_mode_window_resizer.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/base/hit_test.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The observer to observe the overview states in |root_window_|.
class OverviewStatesObserver : public OverviewObserver {
 public:
  OverviewStatesObserver(aura::Window* root_window)
      : root_window_(root_window) {
    Shell::Get()->overview_controller()->AddObserver(this);
  }
  ~OverviewStatesObserver() override {
    Shell::Get()->overview_controller()->RemoveObserver(this);
  }

  // OverviewObserver:
  void OnOverviewModeStarting() override {
    // Reset the value to true.
    overview_animate_when_exiting_ = true;
  }
  void OnOverviewModeEnding(OverviewSession* overview_session) override {
    OverviewGrid* grid = overview_session->GetGridWithRootWindow(root_window_);
    if (!grid)
      return;
    overview_animate_when_exiting_ = grid->should_animate_when_exiting();
  }

  bool overview_animate_when_exiting() const {
    return overview_animate_when_exiting_;
  }

 private:
  bool overview_animate_when_exiting_ = true;
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(OverviewStatesObserver);
};

// The test BubbleDialogDelegateView for bubbles.
class TestBubbleDialogDelegateView : public views::BubbleDialogDelegateView {
 public:
  explicit TestBubbleDialogDelegateView(views::View* anchor_view)
      : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::NONE) {}
  ~TestBubbleDialogDelegateView() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBubbleDialogDelegateView);
};

bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

}  // namespace

class SplitViewControllerTest : public MultiDisplayOverviewAndSplitViewTest {
 public:
  SplitViewControllerTest() = default;
  ~SplitViewControllerTest() override = default;

  // test::AshTestBase:
  void SetUp() override {
    MultiDisplayOverviewAndSplitViewTest::SetUp();
    // Avoid TabletModeController::OnGetSwitchStates() from disabling tablet
    // mode.
    base::RunLoop().RunUntilIdle();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    FpsCounter::SetForceReportZeroAnimationForTest(true);
    PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(true);
  }
  void TearDown() override {
    PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        false);
    FpsCounter::SetForceReportZeroAnimationForTest(false);
    trace_names_.clear();
    MultiDisplayOverviewAndSplitViewTest::TearDown();
  }

  aura::Window* CreateWindow(
      const gfx::Rect& bounds,
      aura::client::WindowType type = aura::client::WINDOW_TYPE_NORMAL) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        new SplitViewTestWindowDelegate, type, -1, bounds);
    return window;
  }

  aura::Window* CreateNonSnappableWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateWindow(bounds);
    window->SetProperty(aura::client::kResizeBehaviorKey,
                        aura::client::kResizeBehaviorNone);
    return window;
  }

  bool IsDividerAnimating() {
    return split_view_controller()->IsDividerAnimating();
  }

  void SkipDividerSnapAnimation() {
    if (!IsDividerAnimating())
      return;
    split_view_controller()->StopAndShoveAnimatedDivider();
    split_view_controller()->EndResizeImpl();
    split_view_controller()->EndTabletSplitViewAfterResizingIfAppropriate();
  }

  void EndSplitView() { split_view_controller()->EndSplitView(); }

  void LongPressOnOverivewButtonTray() {
    ui::GestureEvent event(0, 0, 0, base::TimeTicks(),
                           ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
    StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->overview_button_tray()
        ->OnGestureEvent(&event);
  }

  std::vector<aura::Window*> GetWindowsInOverviewGrids() {
    return Shell::Get()
        ->overview_controller()
        ->GetWindowsListInOverviewGridsForTest();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

  SplitViewDivider* split_view_divider() {
    return split_view_controller()->split_view_divider();
  }

  int divider_position() { return split_view_controller()->divider_position(); }

  float divider_closest_ratio() {
    return split_view_controller()->divider_closest_ratio_;
  }

 protected:
  class SplitViewTestWindowDelegate : public aura::test::TestWindowDelegate {
   public:
    SplitViewTestWindowDelegate() = default;
    ~SplitViewTestWindowDelegate() override = default;

    // aura::test::TestWindowDelegate:
    void OnWindowDestroying(aura::Window* window) override { window->Hide(); }
    void OnWindowDestroyed(aura::Window* window) override { delete this; }
  };

  void CheckForDuplicateTraceName(const char* trace) {
    DCHECK(!base::Contains(trace_names_, trace)) << trace;
    trace_names_.push_back(trace);
  }

  void CheckOverviewEnterExitHistogram(const char* trace,
                                       std::vector<int>&& enter_counts,
                                       std::vector<int>&& exit_counts) {
    CheckForDuplicateTraceName(trace);
    {
      SCOPED_TRACE(trace + std::string(".Enter"));
      CheckOverviewHistogram("Ash.Overview.AnimationSmoothness.Enter",
                             std::move(enter_counts));
    }
    {
      SCOPED_TRACE(trace + std::string(".Exit"));
      CheckOverviewHistogram("Ash.Overview.AnimationSmoothness.Exit",
                             std::move(exit_counts));
    }
  }

  const base::HistogramTester& histograms() const { return histograms_; }

 private:
  void CheckOverviewHistogram(const char* histogram, std::vector<int> counts) {
    // These two events should never happen in this test.
    histograms_.ExpectTotalCount(histogram + std::string(".ClamshellMode"), 0);
    histograms_.ExpectTotalCount(
        histogram + std::string(".SingleClamshellMode"), 0);

    histograms_.ExpectTotalCount(histogram + std::string(".TabletMode"),
                                 counts[0]);
    histograms_.ExpectTotalCount(histogram + std::string(".SplitView"),
                                 counts[1]);
  }
  std::vector<std::string> trace_names_;
  base::HistogramTester histograms_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewControllerTest);
};

class TestWindowStateDelegate : public WindowStateDelegate {
 public:
  TestWindowStateDelegate() = default;
  ~TestWindowStateDelegate() override = default;

  // WindowStateDelegate:
  void OnDragStarted(int component) override { drag_in_progress_ = true; }
  void OnDragFinished(bool cancel, const gfx::PointF& location) override {
    drag_in_progress_ = false;
  }

  bool drag_in_progress() { return drag_in_progress_; }

 private:
  bool drag_in_progress_ = false;
  DISALLOW_COPY_AND_ASSIGN(TestWindowStateDelegate);
};

// Tests the basic functionalities.
TEST_P(SplitViewControllerTest, Basic) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kNoSnap);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_NE(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(window1->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                SplitViewController::LEFT, window1.get()));

  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
  EXPECT_NE(split_view_controller()->right_window(), window1.get());
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(window2->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                SplitViewController::RIGHT, window2.get()));

  EndSplitView();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kNoSnap);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
}

// Tests that the default snapped window is the first window that gets snapped.
TEST_P(SplitViewControllerTest, DefaultSnappedWindow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(window1.get(), split_view_controller()->GetDefaultSnappedWindow());

  EndSplitView();
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(window2.get(), split_view_controller()->GetDefaultSnappedWindow());
}

// Tests that if there are two snapped windows, closing one of them will open
// overview window grid on the closed window side of the screen. If there is
// only one snapped windows, closing the snapped window will end split view mode
// and adjust the overview window grid bounds if the overview mode is active at
// that moment.
TEST_P(SplitViewControllerTest, WindowCloseTest) {
  // 1 - First test one snapped window scenario.
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window0(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  split_view_controller()->SnapWindow(window0.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  // Closing this snapped window should exit split view mode.
  window0.reset();
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  // 2 - Then test two snapped windows scenario.
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::LEFT);

  // Closing one of the two snapped windows will not end split view mode.
  window1.reset();
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kRightSnapped);
  // Since left window was closed, its default snap position changed to RIGHT.
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::RIGHT);
  // Window grid is showing no recent items, and has no windows, but it is still
  // available.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Now close the other snapped window.
  window2.reset();
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kNoSnap);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // 3 - Then test the scenario with more than two windows.
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window5(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window3.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window4.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::LEFT);

  // Close one of the snapped windows.
  window4.reset();
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::LEFT);
  // Now overview window grid can be opened.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Close the other snapped window.
  window3.reset();
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kNoSnap);
  // Test the overview winow grid should still open.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that if there are two snapped windows, minimizing one of them will open
// overview window grid on the minimized window side of the screen. If there is
// only one snapped windows, minimizing the sanpped window will end split view
// mode and adjust the overview window grid bounds if the overview mode is
// active at that moment.
TEST_P(SplitViewControllerTest, MinimizeWindowTest) {
  const gfx::Rect bounds(0, 0, 400, 400);

  // 1 - First test one snapped window scenario.
  std::unique_ptr<aura::Window> window0(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  split_view_controller()->SnapWindow(window0.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  WMEvent minimize_event(WM_EVENT_MINIMIZE);
  WindowState::Get(window0.get())->OnWMEvent(&minimize_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  // 2 - Then test the scenario that has 2 or more windows.
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::LEFT);

  // Minimizing one of the two snapped windows will not end split view mode.
  WindowState::Get(window1.get())->OnWMEvent(&minimize_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kRightSnapped);
  // Since left window was minimized, its default snap position changed to
  // RIGHT.
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::RIGHT);
  // The overview window grid will open.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Now minimize the other snapped window.
  WindowState::Get(window2.get())->OnWMEvent(&minimize_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kNoSnap);
  // The overview window grid is still open.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that if one of the snapped window gets maximized / full-screened, the
// split view mode ends.
TEST_P(SplitViewControllerTest, WindowStateChangeTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  // 1 - First test one snapped window scenario.
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);

  WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  WindowState::Get(window1.get())->OnWMEvent(&maximize_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);

  WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
  WindowState::Get(window1.get())->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  // 2 - Then test two snapped window scenario.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  // Reactivate |window1| because it is the one that we will be maximizing and
  // fullscreening. When |window1| goes out of scope at the end of the test, it
  // will be a full screen window, and if it is not the active window, then the
  // destructor will cause a |DCHECK| failure in |ash::WindowState::Get|.
  wm::ActivateWindow(window1.get());
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);

  // Maximize one of the snapped window will end the split view mode.
  WindowState::Get(window1.get())->OnWMEvent(&maximize_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);

  // Full-screen one of the snapped window will also end the split view mode.
  WindowState::Get(window1.get())->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  // 3 - Test the scenario that part of the screen is a snapped window and part
  // of the screen is the overview window grid.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  ToggleOverview();
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);

  // Maximize the snapped window will end the split view mode and overview mode.
  WindowState::Get(window1.get())->OnWMEvent(&maximize_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  ToggleOverview();
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);

  // Fullscreen the snapped window will end the split view mode and overview
  // mode.
  WindowState::Get(window1.get())->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that if split view mode is active, activate another window will snap
// the window to the non-default side of the screen.
TEST_P(SplitViewControllerTest, WindowActivationTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);

  wm::ActivateWindow(window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);

  wm::ActivateWindow(window3.get());
  EXPECT_EQ(split_view_controller()->right_window(), window3.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
}

// Tests that if split view mode and overview mode are active at the same time,
// i.e., half of the screen is occupied by a snapped window and half of the
// screen is occupied by the overview windows grid, the next activatable window
// will be picked to snap when exiting the overview mode.
TEST_P(SplitViewControllerTest, ExitOverviewTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  ToggleOverview();
  CheckOverviewEnterExitHistogram("EnterInTablet", {1, 0}, {0, 0});

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  // Activate |window1| in preparation to verify that it stays active when
  // overview mode is ended.
  wm::ActivateWindow(window1.get());

  ToggleOverview();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->right_window(), window3.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  CheckOverviewEnterExitHistogram("ExitInSplitView", {1, 0}, {0, 1});
}

// Tests that if split view mode is active when entering overview, the overview
// windows grid should show in the non-default side of the screen, and the
// default snapped window should not be shown in the overview window grid.
TEST_P(SplitViewControllerTest, EnterOverviewModeTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->GetDefaultSnappedWindow(), window1.get());

  ToggleOverview();
  CheckOverviewEnterExitHistogram("EnterInSplitView", {0, 1}, {0, 0});
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_FALSE(
      base::Contains(GetWindowsInOverviewGrids(),
                     split_view_controller()->GetDefaultSnappedWindow()));

  ToggleOverview();
  CheckOverviewEnterExitHistogram("ExitInSplitView", {0, 1}, {0, 1});
}

// Tests that the split divider was created when the split view mode is active
// and destroyed when the split view mode is ended. The split divider should be
// always above the two snapped windows.
TEST_P(SplitViewControllerTest, SplitDividerBasicTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  EXPECT_TRUE(!split_view_divider());
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_TRUE(split_view_divider());
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_TRUE(split_view_divider());
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());

  // Test that activating an non-snappable window ends the split view mode.
  std::unique_ptr<aura::Window> window3(CreateNonSnappableWindow(bounds));
  wm::ActivateWindow(window3.get());
  EXPECT_FALSE(split_view_divider());
}

// Tests that the split divider has correct state after a window is destroyed
// while being dragged from the top.
TEST_P(SplitViewControllerTest,
       DividerSetAsAlwaysOnTopAfterWindowDestroyedDuringDraggingFromTop) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();
  window2->SetProperty(aura::client::kAppType,
                       static_cast<int>(AppType::BROWSER));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  // The divider should start always on top.
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  // The divider should not be always on top while a window is being dragged.
  std::unique_ptr<WindowResizer> resizer =
      CreateWindowResizer(window2.get(), gfx::PointF(400, 0), HTCAPTION,
                          ::wm::WINDOW_MOVE_SOURCE_TOUCH);
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  // If the dragged window is destroyed, the divider should be back to always on
  // top, consistent with if the drag ends gracefully.
  resizer.reset();
  window2.reset();
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  // |SplitViewDivider::is_dragging_window_| should be false, but instead of
  // checking its value directly, we test for what may go wrong if it is true.
  // If |SplitViewDivider::is_dragging_window_| is true, then the following call
  // to |SplitViewController::SnapWindow| will set the divider to always on top,
  // which is fine, but then the call to |wm::ActivateWindow| will change it
  // back to not always on top (see |SplitViewDivider::OnWindowActivated|).
  split_view_controller()->SnapWindow(window3.get(),
                                      SplitViewController::RIGHT);
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  wm::ActivateWindow(window3.get());
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
}

// Tests that the split divider has correct state after a window is destroyed
// while being dragged from overview.
TEST_P(SplitViewControllerTest,
       DividerSetAsAlwaysOnTopAfterWindowDestroyedDuringDraggingFromOverview) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  // The divider should start always on top.
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  // The divider should not be always on top while a window is being dragged.
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  OverviewItem* overview_item =
      overview_session->GetOverviewItemForWindow(window2.get());
  gfx::PointF drag_point = overview_item->target_bounds().CenterPoint();
  overview_session->InitiateDrag(overview_item, drag_point,
                                 /*is_touch_dragging=*/false);
  drag_point.Offset(5.f, 0.f);
  overview_session->Drag(overview_item, drag_point);
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  // If the dragged window is destroyed, the divider should be back to always on
  // top, consistent with if the drag ends gracefully.
  window2.reset();
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  // |SplitViewDivider::is_dragging_window_| should be false, but instead of
  // checking its value directly, we test for what may go wrong if it is true.
  // If |SplitViewDivider::is_dragging_window_| is true, then the following call
  // to |SplitViewController::SnapWindow| will set the divider to always on top,
  // which is fine, but then the call to |wm::ActivateWindow| will change it
  // back to not always on top (see |SplitViewDivider::OnWindowActivated|).
  split_view_controller()->SnapWindow(window3.get(),
                                      SplitViewController::RIGHT);
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  wm::ActivateWindow(window3.get());
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
}

// Tests that the split divider has correct state after a window drag from
// overview is canceled.
TEST_P(SplitViewControllerTest,
       DividerSetAsAlwaysOnTopAfterWindowDragFromOverviewReset) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  // The divider should start always on top.
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  // The divider should not be always on top while a window is being dragged.
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  OverviewItem* overview_item =
      overview_session->GetOverviewItemForWindow(window2.get());
  gfx::PointF drag_point = overview_item->target_bounds().CenterPoint();
  overview_session->InitiateDrag(overview_item, drag_point,
                                 /*is_touch_dragging=*/false);
  drag_point.Offset(5.f, 0.f);
  overview_session->Drag(overview_item, drag_point);
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  // If the drag is canceled, the divider should be back to always on top,
  // consistent with if the drag ends gracefully.
  overview_session->ResetDraggedWindowGesture();
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  // |SplitViewDivider::is_dragging_window_| should be false, but instead of
  // checking its value directly, we test for what may go wrong if it is true.
  // If |SplitViewDivider::is_dragging_window_| is true, then the following call
  // to |SplitViewController::SnapWindow| will set the divider to always on top,
  // which is fine, but then the call to |wm::ActivateWindow| will change it
  // back to not always on top (see |SplitViewDivider::OnWindowActivated|).
  split_view_controller()->SnapWindow(window3.get(),
                                      SplitViewController::RIGHT);
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
  wm::ActivateWindow(window3.get());
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
}

// Verifys that the bounds of the two windows in splitview are as expected.
TEST_P(SplitViewControllerTest, SplitDividerWindowBounds) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  ASSERT_TRUE(split_view_divider());

  // Verify with two freshly snapped windows are roughly the same width (off by
  // one pixel at most due to the display maybe being even and the divider being
  // a fixed odd pixel width).
  int window1_width = window1->GetBoundsInScreen().width();
  int window2_width = window2->GetBoundsInScreen().width();
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(window1.get()).width();
  EXPECT_NEAR(window1_width, window2_width, 1);
  EXPECT_EQ(screen_width,
            window1_width + divider_bounds.width() + window2_width);

  // Drag the divider to a position two thirds of the screen size. Verify window
  // 1 is wider than window 2.
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->DragMouseTo(screen_width * 0.67f, 0);
  SkipDividerSnapAnimation();
  window1_width = window1->GetBoundsInScreen().width();
  window2_width = window2->GetBoundsInScreen().width();
  const int old_window1_width = window1_width;
  const int old_window2_width = window2_width;
  EXPECT_GT(window1_width, 2 * window2_width);
  EXPECT_EQ(screen_width,
            window1_width + divider_bounds.width() + window2_width);

  // Drag the divider to a position close to two thirds of the screen size.
  // Verify the divider snaps to two thirds of the screen size, and the windows
  // remain the same size as previously.
  divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->DragMouseTo(screen_width * 0.7f, 0);
  SkipDividerSnapAnimation();
  window1_width = window1->GetBoundsInScreen().width();
  window2_width = window2->GetBoundsInScreen().width();
  EXPECT_EQ(window1_width, old_window1_width);
  EXPECT_EQ(window2_width, old_window2_width);

  // Drag the divider to a position one third of the screen size. Verify window
  // 1 is wider than window 2.
  divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->DragMouseTo(screen_width * 0.33f, 0);
  SkipDividerSnapAnimation();
  window1_width = window1->GetBoundsInScreen().width();
  window2_width = window2->GetBoundsInScreen().width();
  EXPECT_GT(window2_width, 2 * window1_width);
  EXPECT_EQ(screen_width,
            window1_width + divider_bounds.width() + window2_width);

  // Verify that the left window from dragging the divider to two thirds of the
  // screen size is roughly the same size as the right window after dragging the
  // divider to one third of the screen size, and vice versa.
  EXPECT_NEAR(window1_width, old_window2_width, 1);
  EXPECT_NEAR(window2_width, old_window1_width, 1);
}

// Tests that the bounds of the snapped windows and divider are adjusted when
// the screen display configuration changes.
TEST_P(SplitViewControllerTest, DisplayConfigurationChangeTest) {
  UpdateDisplay("407x400");
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  const gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1| and |window2| has the same width and height after snap.
  EXPECT_NEAR(bounds_window1.width(), bounds_window2.width(), 1);
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
  EXPECT_EQ(bounds_divider.height(), bounds_window1.height());

  // Test that |window1|, divider, |window2| are aligned properly.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());

  // Now change the display configuration.
  UpdateDisplay("507x500");
  const gfx::Rect new_bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect new_bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect new_bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that the new bounds are different with the old ones.
  EXPECT_NE(bounds_window1, new_bounds_window1);
  EXPECT_NE(bounds_window2, new_bounds_window2);
  EXPECT_NE(bounds_divider, new_bounds_divider);

  // Test that |window1|, divider, |window2| are still aligned properly.
  EXPECT_EQ(new_bounds_divider.x(),
            new_bounds_window1.x() + new_bounds_window1.width());
  EXPECT_EQ(new_bounds_window2.x(),
            new_bounds_divider.x() + new_bounds_divider.width());
}

// Tests that the bounds of the snapped windows and divider are adjusted when
// the internal screen display configuration changes.
TEST_P(SplitViewControllerTest, InternalDisplayConfigurationChangeTest) {
  UpdateDisplay("407x400");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  const gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1| and |window2| has the same width and height after snap.
  EXPECT_NEAR(bounds_window1.width(), bounds_window2.width(), 1);
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
  EXPECT_EQ(bounds_divider.height(), bounds_window1.height());

  // Test that |window1|, divider, |window2| are aligned properly.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());

  // Now change the display configuration.
  UpdateDisplay("507x500");
  const gfx::Rect new_bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect new_bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect new_bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that the new bounds are different with the old ones.
  EXPECT_NE(bounds_window1, new_bounds_window1);
  EXPECT_NE(bounds_window2, new_bounds_window2);
  EXPECT_NE(bounds_divider, new_bounds_divider);

  // Test that |window1|, divider, |window2| are still aligned properly.
  EXPECT_EQ(new_bounds_divider.x(),
            new_bounds_window1.x() + new_bounds_window1.width());
  EXPECT_EQ(new_bounds_window2.x(),
            new_bounds_divider.x() + new_bounds_divider.width());
}

// Test that if the internal screen display configuration changes during the
// divider snap animation, then this animation stops, and the bounds of the
// snapped windows and divider are adjusted as normal.
TEST_P(SplitViewControllerTest,
       InternalDisplayConfigurationChangeDuringDividerSnap) {
  UpdateDisplay("407x400");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  const gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1| and |window2| has the same width and height after snap.
  EXPECT_NEAR(bounds_window1.width(), bounds_window2.width(), 1);
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
  EXPECT_EQ(bounds_divider.height(), bounds_window1.height());

  // Test that |window1|, divider, |window2| are aligned properly.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());

  // Drag the divider to trigger the snap animation.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->set_current_screen_location(divider_center);
  GetEventGenerator()->DragMouseBy(20, 0);
  ASSERT_TRUE(IsDividerAnimating());
  const gfx::Rect animation_start_bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect animation_start_bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect animation_start_bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Change the display configuration and check that the snap animation stops.
  UpdateDisplay("507x500");
  EXPECT_FALSE(IsDividerAnimating());
  const gfx::Rect new_bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect new_bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect new_bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that the new bounds are different with the old ones.
  EXPECT_NE(bounds_window1, new_bounds_window1);
  EXPECT_NE(bounds_window2, new_bounds_window2);
  EXPECT_NE(bounds_divider, new_bounds_divider);

  // Test that the new bounds are also different with the ones from the start of
  // the divider snap animation.
  EXPECT_NE(bounds_window1, animation_start_bounds_window1);
  EXPECT_NE(bounds_window2, animation_start_bounds_window2);
  EXPECT_NE(bounds_divider, animation_start_bounds_divider);

  // Test that |window1|, divider, |window2| are still aligned properly.
  EXPECT_EQ(new_bounds_divider.x(),
            new_bounds_window1.x() + new_bounds_window1.width());
  EXPECT_EQ(new_bounds_window2.x(),
            new_bounds_divider.x() + new_bounds_divider.width());
}

// Test that if the internal screen display configuration changes during the
// divider snap animation, and if the adjusted divider bounds place it at the
// left edge of the screen, then split view ends.
TEST_P(SplitViewControllerTest,
       InternalDisplayConfigurationChangeDuringDividerSnapToLeft) {
  UpdateDisplay("407x400");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  const gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1| and |window2| has the same width and height after snap.
  EXPECT_NEAR(bounds_window1.width(), bounds_window2.width(), 1);
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
  EXPECT_EQ(bounds_divider.height(), bounds_window1.height());

  // Test that |window1|, divider, |window2| are aligned properly.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());

  // Drag the divider to end split view pending the snap animation.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->set_current_screen_location(divider_center);
  GetEventGenerator()->DragMouseBy(20 - bounds_window1.width(), 0);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  ASSERT_TRUE(IsDividerAnimating());

  // Change the display configuration and check that split view ends.
  UpdateDisplay("507x500");
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test that if the internal screen display configuration changes during the
// divider snap animation, and if the adjusted divider bounds place it at the
// right edge of the screen, then split view ends.
TEST_P(SplitViewControllerTest,
       InternalDisplayConfigurationChangeDuringDividerSnapToRight) {
  UpdateDisplay("407x400");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  const gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1| and |window2| has the same width and height after snap.
  EXPECT_NEAR(bounds_window1.width(), bounds_window2.width(), 1);
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
  EXPECT_EQ(bounds_divider.height(), bounds_window1.height());

  // Test that |window1|, divider, |window2| are aligned properly.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());

  // Drag the divider to end split view pending the snap animation.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->set_current_screen_location(divider_center);
  GetEventGenerator()->DragMouseBy(bounds_window2.width() - 20, 0);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  ASSERT_TRUE(IsDividerAnimating());

  // Change the display configuration and check that split view ends.
  UpdateDisplay("507x500");
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Verify the left and right windows get swapped when SwapWindows is called or
// the divider is double clicked.
TEST_P(SplitViewControllerTest, SwapWindows) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  ASSERT_EQ(split_view_controller()->left_window(), window1.get());
  ASSERT_EQ(split_view_controller()->right_window(), window2.get());

  gfx::Rect left_bounds = window1->GetBoundsInScreen();
  gfx::Rect right_bounds = window2->GetBoundsInScreen();

  // Verify that after swapping windows, the windows and their bounds have been
  // swapped.
  split_view_controller()->SwapWindows();
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_EQ(left_bounds, window2->GetBoundsInScreen());
  EXPECT_EQ(right_bounds, window1->GetBoundsInScreen());

  // End split view mode and snap the window to RIGHT first, verify the function
  // SwapWindows() still works properly.
  EndSplitView();
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);
  ASSERT_EQ(split_view_controller()->right_window(), window1.get());
  ASSERT_EQ(split_view_controller()->left_window(), window2.get());

  left_bounds = window2->GetBoundsInScreen();
  right_bounds = window1->GetBoundsInScreen();

  split_view_controller()->SwapWindows();
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
  EXPECT_EQ(left_bounds, window1->GetBoundsInScreen());
  EXPECT_EQ(right_bounds, window2->GetBoundsInScreen());

  // Perform a double click on the divider center.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->set_current_screen_location(divider_center);
  GetEventGenerator()->DoubleClickLeftButton();

  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_EQ(left_bounds, window2->GetBoundsInScreen());
  EXPECT_EQ(right_bounds, window1->GetBoundsInScreen());
}

// Verify the left and right windows get swapped when the divider is double
// tapped. SwapWindows() contains a long code comment that shows it is worth
// having separate tests for double clicking and double tapping the divider.
TEST_P(SplitViewControllerTest, DoubleTapDivider) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  ASSERT_EQ(split_view_controller()->left_window(), window1.get());
  ASSERT_EQ(split_view_controller()->right_window(), window2.get());

  gfx::Rect left_bounds = window1->GetBoundsInScreen();
  gfx::Rect right_bounds = window2->GetBoundsInScreen();

  // Perform a double tap on the divider center.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->GestureTapAt(divider_center);
  GetEventGenerator()->GestureTapAt(divider_center);

  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_EQ(left_bounds, window2->GetBoundsInScreen());
  EXPECT_EQ(right_bounds, window1->GetBoundsInScreen());
}

// Verify the left and right windows do not get swapped when the divider is
// dragged and double clicked.
TEST_P(SplitViewControllerTest, DragAndDoubleClickDivider) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  ASSERT_EQ(split_view_controller()->left_window(), window1.get());
  ASSERT_EQ(split_view_controller()->right_window(), window2.get());

  // Drag the divider and double click it before the snap animation moves it.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->set_current_screen_location(divider_center);
  GetEventGenerator()->DragMouseBy(20, 0);
  GetEventGenerator()->DoubleClickLeftButton();

  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
}

// Verify the left and right windows do not get swapped when the divider is
// dragged and double tapped.
TEST_P(SplitViewControllerTest, DragAndDoubleTapDivider) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  ASSERT_EQ(split_view_controller()->left_window(), window1.get());
  ASSERT_EQ(split_view_controller()->right_window(), window2.get());

  // Drag the divider and double tap it before the snap animation moves it.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->set_current_screen_location(divider_center);
  const gfx::Point drag_destination = divider_center + gfx::Vector2d(20, 0);
  GetEventGenerator()->DragMouseTo(drag_destination);
  GetEventGenerator()->GestureTapAt(drag_destination);
  GetEventGenerator()->GestureTapAt(drag_destination);

  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
}

// Verify overview does not steal focus from a split view window when trading
// places with it.
TEST_P(SplitViewControllerTest, OverviewNotStealFocusOnSwapWindows) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  ToggleOverview();
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);
  wm::ActivateWindow(window2.get());
  split_view_controller()->SwapWindows();
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Verify that you cannot start dragging the divider during its snap animation.
TEST_P(SplitViewControllerTest, StartDraggingDividerDuringSnapAnimation) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  ASSERT_EQ(split_view_controller()->left_window(), window1.get());
  ASSERT_EQ(split_view_controller()->right_window(), window2.get());

  // Drag the divider and then try to start dragging it again without waiting
  // for the snap animation.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->set_current_screen_location(divider_center);
  GetEventGenerator()->DragMouseBy(20, 0);
  GetEventGenerator()->PressLeftButton();
  EXPECT_FALSE(split_view_controller()->is_resizing());
  GetEventGenerator()->ReleaseLeftButton();
}

TEST_P(SplitViewControllerTest, LongPressEntersSplitView) {
  // Tests that with no active windows, split view does not get activated.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  wm::ActivateWindow(window1.get());

  // Tests that with split view gets activated with an active window.
  LongPressOnOverivewButtonTray();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
}

// Verify that when in split view mode with either one snapped or two snapped
// windows, split view mode gets exited when the overview button gets a long
// press event.
TEST_P(SplitViewControllerTest, LongPressExitsSplitView) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  // Snap |window1| to the left.
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());

  // Verify that by long pressing on the overview button tray with left snapped
  // window, split view mode gets exited and the left window (|window1|) is the
  // current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(window1.get(), window_util::GetActiveWindow());

  // Snap |window1| to the right.
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());

  // Verify that by long pressing on the overview button tray with right snapped
  // window, split view mode gets exited and the right window (|window1|) is the
  // current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(window1.get(), window_util::GetActiveWindow());

  // Snap two windows and activate the left window, |window1|.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  wm::ActivateWindow(window1.get());
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());

  // Verify that by long pressing on the overview button tray with two snapped
  // windows, split view mode gets exited.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(window1.get(), window_util::GetActiveWindow());

  // Snap two windows and activate the right window, |window2|.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  wm::ActivateWindow(window2.get());
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());

  // Verify that by long pressing on the overview button tray with two snapped
  // windows, split view mode gets exited, and the activated window in splitview
  // is the current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(window2.get(), window_util::GetActiveWindow());
}

// Verify that if a window with a transient child which is not snappable is
// activated, and the the overview tray is long pressed, we will enter splitview
// with the transient parent snapped.
TEST_P(SplitViewControllerTest, LongPressEntersSplitViewWithTransientChild) {
  // Add two windows with one being a transient child of the first.
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> parent(CreateWindow(bounds));
  std::unique_ptr<aura::Window> child(
      CreateWindow(bounds, aura::client::WINDOW_TYPE_POPUP));
  wm::AddTransientChild(parent.get(), child.get());
  wm::ActivateWindow(parent.get());
  wm::ActivateWindow(child.get());

  // Verify that long press will snap the focused transient child's parent.
  LongPressOnOverivewButtonTray();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->GetDefaultSnappedWindow(), parent.get());
}

TEST_P(SplitViewControllerTest, LongPressExitsSplitViewWithTransientChild) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> left_window(CreateWindow(bounds));
  std::unique_ptr<aura::Window> right_window(CreateWindow(bounds));
  wm::ActivateWindow(left_window.get());
  wm::ActivateWindow(right_window.get());

  ToggleOverview();
  split_view_controller()->SnapWindow(left_window.get(),
                                      SplitViewController::LEFT);
  split_view_controller()->SnapWindow(right_window.get(),
                                      SplitViewController::RIGHT);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());

  // Add a transient child to |right_window|, and activate it.
  aura::Window* transient_child =
      aura::test::CreateTestWindowWithId(0, right_window.get());
  ::wm::AddTransientChild(right_window.get(), transient_child);
  wm::ActivateWindow(transient_child);

  // Verify that by long pressing on the overview button tray, split view mode
  // gets exited and the window which contained |transient_child| is the
  // current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(right_window.get(), window_util::GetActiveWindow());
}

// Verify that split view mode get activated when long pressing on the overview
// button while in overview mode iff we have at least one window.
TEST_P(SplitViewControllerTest, LongPressInOverviewMode) {
  ToggleOverview();
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  ASSERT_FALSE(split_view_controller()->InSplitViewMode());
  CheckOverviewEnterExitHistogram("EnterInTablet", {0, 0}, {0, 0});

  // Nothing happens if there are no windows.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Verify that with a window, a long press on the overview button tray will
  // enter splitview.
  std::unique_ptr<aura::Window> window(CreateWindow(gfx::Rect(0, 0, 400, 400)));
  wm::ActivateWindow(window.get());
  CheckOverviewEnterExitHistogram("ExitByActivation", {0, 0}, {0, 0});

  ToggleOverview();
  CheckOverviewEnterExitHistogram("EnterInTablet2", {1, 0}, {0, 0});
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  ASSERT_FALSE(split_view_controller()->InSplitViewMode());

  LongPressOnOverivewButtonTray();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(window.get(), split_view_controller()->left_window());
  // This scenario should not trigger animation.
  CheckOverviewEnterExitHistogram("NoTransition", {1, 0}, {0, 0});
}

TEST_P(SplitViewControllerTest, LongPressWithUnsnappableWindow) {
  // Add an unsnappable window and a regular window.
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> unsnappable_window(
      CreateNonSnappableWindow(bounds));
  ASSERT_FALSE(split_view_controller()->InSplitViewMode());
  std::unique_ptr<aura::Window> regular_window(CreateWindow(bounds));
  wm::ActivateWindow(regular_window.get());
  wm::ActivateWindow(unsnappable_window.get());
  ASSERT_EQ(unsnappable_window.get(), window_util::GetActiveWindow());

  // Verify split view is not activated when long press occurs when active
  // window is unsnappable.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // Verify split view is not activated when long press occurs in overview mode
  // and the most recent window is unsnappable.
  ToggleOverview();
  ASSERT_TRUE(Shell::Get()
                  ->mru_window_tracker()
                  ->BuildWindowForCycleList(kActiveDesk)
                  .size() > 0);
  ASSERT_EQ(unsnappable_window.get(),
            Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(
                kActiveDesk)[0]);
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Tests that long press works even if the window is minimized.
TEST_P(SplitViewControllerTest, LongPressWithMinimizedWindow) {
  std::unique_ptr<aura::Window> window(CreateWindow(gfx::Rect(400, 400)));
  WindowState::Get(window.get())->Minimize();

  LongPressOnOverivewButtonTray();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
}

// Test the rotation functionalities in split view mode.
TEST_P(SplitViewControllerTest, RotationTest) {
  UpdateDisplay("807x407");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  // Set the screen orientation to LANDSCAPE_PRIMARY.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapePrimary);

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test |window1|, divider and |window2| are aligned horizontally.
  // |window1| is on the left, then the divider, and then |window2|.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());
  EXPECT_EQ(bounds_window1.height(), bounds_divider.height());
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitPrimary);

  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1|, divider, |window2| are now aligned vertically.
  // |window1| is on the top, then the divider, and then |window2|.
  EXPECT_EQ(bounds_divider.y(), bounds_window1.y() + bounds_window1.height());
  EXPECT_EQ(bounds_window2.y(), bounds_divider.y() + bounds_divider.height());
  EXPECT_EQ(bounds_window1.width(), bounds_divider.width());
  EXPECT_EQ(bounds_window1.width(), bounds_window2.width());

  // Rotate the screen by 180 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapeSecondary);

  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1|, divider, |window2| are now aligned horizontally.
  // |window2| is on the left, then the divider, and then |window1|.
  EXPECT_EQ(bounds_divider.x(), bounds_window2.x() + bounds_window2.width());
  EXPECT_EQ(bounds_window1.x(), bounds_divider.x() + bounds_divider.width());
  EXPECT_EQ(bounds_window1.height(), bounds_divider.height());
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());

  // Rotate the screen by 90 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitSecondary);
  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1|, divider, |window2| are now aligned vertically.
  // |window2| is on the top, then the divider, and then |window1|.
  EXPECT_EQ(bounds_divider.y(), bounds_window2.y() + bounds_window2.height());
  EXPECT_EQ(bounds_window1.y(), bounds_divider.y() + bounds_divider.height());
  EXPECT_EQ(bounds_window1.width(), bounds_divider.width());
  EXPECT_EQ(bounds_window1.width(), bounds_window2.width());

  // Rotate the screen back to 0 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapePrimary);
  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test |window1|, divider and |window2| are aligned horizontally.
  // |window1| is on the left, then the divider, and then |window2|.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());
  EXPECT_EQ(bounds_window1.height(), bounds_divider.height());
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
}

// Test that if the split view mode is active when exiting tablet mode, we
// should also end split view mode.
TEST_P(SplitViewControllerTest, ExitTabletModeEndSplitView) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test that |SplitViewController::CanSnapWindow| checks that the minimum size
// of the window fits into the left or top, with the default divider position.
// (If the work area length is odd, then the right or bottom will be one pixel
// larger.)
TEST_P(SplitViewControllerTest, SnapWindowWithMinimumSizeTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window1.get()));

  UpdateDisplay("800x600");
  aura::test::TestWindowDelegate* delegate =
      static_cast<aura::test::TestWindowDelegate*>(window1->delegate());
  delegate->set_minimum_size(gfx::Size(396, 0));
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window1.get()));
  delegate->set_minimum_size(gfx::Size(397, 0));
  EXPECT_FALSE(split_view_controller()->CanSnapWindow(window1.get()));

  UpdateDisplay("799x600");
  delegate->set_minimum_size(gfx::Size(395, 0));
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window1.get()));
  delegate->set_minimum_size(gfx::Size(396, 0));
  EXPECT_FALSE(split_view_controller()->CanSnapWindow(window1.get()));
}

// Tests that the snapped window can not be moved outside of work area when its
// minimum size is larger than its current desired resizing bounds.
TEST_P(SplitViewControllerTest, ResizingSnappedWindowWithMinimumSizeTest) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  const gfx::Rect bounds(0, 0, 300, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate1 =
      static_cast<aura::test::TestWindowDelegate*>(window1->delegate());

  // Set the screen orientation to LANDSCAPE_PRIMARY
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapePrimary);

  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window1.get()));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.4f, display_bounds.height()));
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());

  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  gfx::Point resize_point(display_bounds.width() * 0.33f, 0);
  split_view_controller()->Resize(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow", 1);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow",
      0);

  gfx::Rect snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::LEFT, window1.get());
  // The snapped window bounds can't be pushed outside of the display area.
  EXPECT_EQ(snapped_window_bounds.x(), display_bounds.x());
  EXPECT_EQ(snapped_window_bounds.width(),
            window1->delegate()->GetMinimumSize().width());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  split_view_controller()->EndResize(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow", 1);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow",
      1);

  SkipDividerSnapAnimation();
  EndSplitView();

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitPrimary);

  display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width(), display_bounds.height() * 0.4f));
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window1.get()));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  resize_point.SetPoint(0, display_bounds.height() * 0.33f);
  split_view_controller()->Resize(resize_point);

  snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::LEFT, window1.get());
  EXPECT_EQ(snapped_window_bounds.y(), display_bounds.y());
  EXPECT_EQ(snapped_window_bounds.height(),
            window1->delegate()->GetMinimumSize().height());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  split_view_controller()->EndResize(resize_point);
  SkipDividerSnapAnimation();
  EndSplitView();

  // Rotate the screen by 180 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapeSecondary);

  display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.4f, display_bounds.height()));
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window1.get()));
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());

  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  resize_point.SetPoint(display_bounds.width() * 0.33f, 0);
  split_view_controller()->Resize(resize_point);

  snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::RIGHT, window1.get());
  EXPECT_EQ(snapped_window_bounds.x(), display_bounds.x());
  EXPECT_EQ(snapped_window_bounds.width(),
            window1->delegate()->GetMinimumSize().width());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  split_view_controller()->EndResize(resize_point);
  SkipDividerSnapAnimation();
  EndSplitView();

  // Rotate the screen by 90 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitSecondary);

  display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width(), display_bounds.height() * 0.4f));
  EXPECT_TRUE(split_view_controller()->CanSnapWindow(window1.get()));
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());

  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  resize_point.SetPoint(0, display_bounds.height() * 0.33f);
  split_view_controller()->Resize(resize_point);

  snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::RIGHT, window1.get());
  EXPECT_EQ(snapped_window_bounds.y(), display_bounds.y());
  EXPECT_EQ(snapped_window_bounds.height(),
            window1->delegate()->GetMinimumSize().height());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  split_view_controller()->EndResize(resize_point);
  SkipDividerSnapAnimation();
  EndSplitView();
}

// Tests that the divider should not be moved to a position that is smaller than
// the snapped window's minimum size after resizing.
TEST_P(SplitViewControllerTest,
       DividerPositionOnResizingSnappedWindowWithMinimumSizeTest) {
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate1 =
      static_cast<aura::test::TestWindowDelegate*>(window1->delegate());
  ui::test::EventGenerator* generator = GetEventGenerator();
  EXPECT_EQ(OrientationLockType::kLandscapePrimary,
            GetCurrentScreenOrientation());
  gfx::Rect workarea_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());

  // Snap the divider to one third position when there is only left window with
  // minimum size larger than one third of the display's width. The divider
  // should be snapped to the middle position after dragging.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  delegate1->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.33f, 0));
  SkipDividerSnapAnimation();
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Snap the divider to two third position, it should be kept at there after
  // dragging.
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  SkipDividerSnapAnimation();
  EXPECT_GT(divider_position(), 0.5f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.67f * workarea_bounds.width());
  EndSplitView();

  // Snap the divider to two third position when there is only right window with
  // minium size larger than one third of the display's width. The divider
  // should be snapped to the middle position after dragging.
  delegate1->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  SkipDividerSnapAnimation();
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Snap the divider to one third position, it should be kept at there after
  // dragging.
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.33f, 0));
  SkipDividerSnapAnimation();
  EXPECT_GT(divider_position(), 0);
  EXPECT_LE(divider_position(), 0.33f * workarea_bounds.width());
  EndSplitView();

  // Snap the divider to one third position when there are both left and right
  // snapped windows with the same minimum size larger than one third of the
  // display's width. The divider should be snapped to the middle position after
  // dragging.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate2 =
      static_cast<aura::test::TestWindowDelegate*>(window2->delegate());
  delegate2->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.33f, 0));
  SkipDividerSnapAnimation();
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Snap the divider to two third position, it should be snapped to the middle
  // position after dragging.
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  SkipDividerSnapAnimation();
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());
  EndSplitView();
}

// Tests that the divider and snapped windows bounds should be updated if
// snapping a new window with minimum size, which is larger than the bounds
// of its snap position.
TEST_P(SplitViewControllerTest,
       DividerPositionWithWindowMinimumSizeOnSnapTest) {
  const gfx::Rect bounds(0, 0, 200, 300);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  const gfx::Rect workarea_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());

  // Divider should be moved to the middle at the beginning.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ASSERT_TRUE(split_view_divider());
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Drag the divider to two-third position.
  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  SkipDividerSnapAnimation();
  EXPECT_GT(divider_position(), 0.5f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.67f * workarea_bounds.width());

  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate2 =
      static_cast<aura::test::TestWindowDelegate*>(window2->delegate());
  delegate2->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());
}

// Test that if display configuration changes in lock screen, the split view
// mode doesn't end.
TEST_P(SplitViewControllerTest, DoNotEndSplitViewInLockScreen) {
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  UpdateDisplay("800x400");
  const gfx::Rect bounds(0, 0, 200, 300);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);

  // Now lock the screen.
  GetSessionControllerClient()->LockScreen();
  // Change display configuration. Split view mode is still active.
  UpdateDisplay("400x800");
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);

  // Now unlock the screen.
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
}

// Test that when split view and overview are both active when a new window is
// added to the window hierarchy, overview is not ended.
TEST_P(SplitViewControllerTest, NewWindowTest) {
  const gfx::Rect bounds(0, 0, 200, 300);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ToggleOverview();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Now new a window. Test it won't end the overview mode
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that when split view ends because of a transition from tablet mode to
// laptop mode during a resize operation, drags are properly completed.
TEST_P(SplitViewControllerTest, ExitTabletModeDuringResizeCompletesDrags) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  auto* w1_state = WindowState::Get(window1.get());
  auto* w2_state = WindowState::Get(window2.get());

  // Setup delegates
  auto* window_state_delegate1 = new TestWindowStateDelegate();
  auto* window_state_delegate2 = new TestWindowStateDelegate();
  w1_state->SetDelegate(base::WrapUnique(window_state_delegate1));
  w2_state->SetDelegate(base::WrapUnique(window_state_delegate2));

  // Set up windows.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  // Start a drag but don't release the mouse button.
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(window1.get()).width();
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseTo(screen_width * 0.67f, 0);

  // Drag is started for both windows.
  EXPECT_TRUE(window_state_delegate1->drag_in_progress());
  EXPECT_TRUE(window_state_delegate2->drag_in_progress());
  EXPECT_NE(nullptr, w1_state->drag_details());
  EXPECT_NE(nullptr, w2_state->drag_details());

  // End tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);

  // Drag is ended for both windows.
  EXPECT_EQ(nullptr, w1_state->drag_details());
  EXPECT_EQ(nullptr, w2_state->drag_details());
  EXPECT_FALSE(window_state_delegate1->drag_in_progress());
  EXPECT_FALSE(window_state_delegate2->drag_in_progress());
}

// Tests that when a single window is present in split view mode is minimized
// during a resize operation, then drags are properly completed.
TEST_P(SplitViewControllerTest,
       MinimizeSingleWindowDuringResizeCompletesDrags) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  auto* w1_state = WindowState::Get(window1.get());

  // Setup delegate
  auto* window_state_delegate1 = new TestWindowStateDelegate();
  w1_state->SetDelegate(base::WrapUnique(window_state_delegate1));

  // Set up window.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);

  // Start a drag but don't release the mouse button.
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParentForActiveDeskContainer(
          window1.get())
          .width();
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseTo(screen_width * 0.67f, 0);

  // Drag is started.
  EXPECT_TRUE(window_state_delegate1->drag_in_progress());
  EXPECT_NE(nullptr, w1_state->drag_details());

  // Minimize the window.
  WMEvent minimize_event(WM_EVENT_MINIMIZE);
  WindowState::Get(window1.get())->OnWMEvent(&minimize_event);

  // Drag is ended.
  EXPECT_FALSE(window_state_delegate1->drag_in_progress());
  EXPECT_EQ(nullptr, w1_state->drag_details());
}

// Tests that when two windows are present in split view mode and one of them
// is minimized during a resize, then drags are properly completed.
TEST_P(SplitViewControllerTest,
       MinimizeOneOfTwoWindowsDuringResizeCompletesDrags) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  auto* w1_state = WindowState::Get(window1.get());
  auto* w2_state = WindowState::Get(window2.get());

  // Setup delegates
  auto* window_state_delegate1 = new TestWindowStateDelegate();
  auto* window_state_delegate2 = new TestWindowStateDelegate();
  w1_state->SetDelegate(base::WrapUnique(window_state_delegate1));
  w2_state->SetDelegate(base::WrapUnique(window_state_delegate2));

  // Set up windows.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  // Start a drag but don't release the mouse button.
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(window1.get()).width();
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseTo(screen_width * 0.67f, 0);

  // Drag is started for both windows.
  EXPECT_TRUE(window_state_delegate1->drag_in_progress());
  EXPECT_TRUE(window_state_delegate2->drag_in_progress());
  EXPECT_NE(nullptr, w1_state->drag_details());
  EXPECT_NE(nullptr, w2_state->drag_details());

  // Minimize the left window.
  WMEvent minimize_event(WM_EVENT_MINIMIZE);
  WindowState::Get(window1.get())->OnWMEvent(&minimize_event);

  // Drag is ended just for the left window.
  EXPECT_FALSE(window_state_delegate1->drag_in_progress());
  EXPECT_TRUE(window_state_delegate2->drag_in_progress());
  EXPECT_EQ(nullptr, w1_state->drag_details());
  EXPECT_NE(nullptr, w2_state->drag_details());
}

// Test that when a snapped window's resizablity property change from resizable
// to unresizable, the split view mode is ended.
TEST_P(SplitViewControllerTest, ResizabilityChangeTest) {
  const gfx::Rect bounds(0, 0, 200, 300);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  window1->SetProperty(aura::client::kResizeBehaviorKey,
                       aura::client::kResizeBehaviorNone);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Tests that shadows on windows disappear when the window is snapped, and
// reappear when unsnapped.
TEST_P(SplitViewControllerTest, ShadowDisappearsWhenSnapped) {
  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ::wm::ShadowController* shadow_controller = Shell::Get()->shadow_controller();
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window1.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window2.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window3.get()));

  // Snap |window1| to the left. Its shadow should disappear.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window1.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window2.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window3.get()));

  // Snap |window2| to the right. Its shadow should also disappear.
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window1.get()));
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window2.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window3.get()));

  // Snap |window3| to the right. Its shadow should disappear and |window2|'s
  // shadow should reappear.
  split_view_controller()->SnapWindow(window3.get(),
                                      SplitViewController::RIGHT);
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window1.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window2.get()));
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window3.get()));
}

// Tests that if snapping a window causes overview to end (e.g., select two
// windows in overview mode to snap to both side of the screen), or toggle
// overview to end overview causes a window to snap, we should not have the
// exiting animation.
TEST_P(SplitViewControllerTest, OverviewExitAnimationTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  // 1) For normal toggle overview case, we should have animation when
  // exiting overview.
  std::unique_ptr<OverviewStatesObserver> overview_observer =
      std::make_unique<OverviewStatesObserver>(window1->GetRootWindow());
  ToggleOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  ToggleOverview();
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());
  CheckOverviewEnterExitHistogram("NormalEnterExit", {1, 0}, {1, 0});

  // 2) If overview is ended because of activating a window:
  ToggleOverview();
  // It will end overview.
  wm::ActivateWindow(window1.get());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());
  CheckOverviewEnterExitHistogram("EnterExitByActivation", {2, 0}, {2, 0});

  // 3) If overview is ended because of snapping a window:
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  // Reset the observer as we'll need the OverviewStatesObserver to be added to
  // to ShellObserver list after SplitViewController.
  overview_observer.reset(new OverviewStatesObserver(window1->GetRootWindow()));
  ToggleOverview();  // Start overview.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  // Test |overview_animate_when_exiting_| has been properly reset.
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());
  CheckOverviewEnterExitHistogram("EnterInSplitView", {2, 1}, {2, 0});

  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_FALSE(overview_observer->overview_animate_when_exiting());
  CheckOverviewEnterExitHistogram("ExitBySnap", {2, 1}, {2, 1});

  // 4) If ending overview causes a window to snap:
  ToggleOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  // Test |overview_animate_when_exiting_| has been properly reset.
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());
  CheckOverviewEnterExitHistogram("EnterInSplitView2", {2, 2}, {2, 1});

  ToggleOverview();
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_FALSE(overview_observer->overview_animate_when_exiting());
  CheckOverviewEnterExitHistogram("ExitInSplitView", {2, 2}, {2, 2});
}

// Test the window state is normally maximized on splitview end, except when we
// end it from home launcher.
TEST_P(SplitViewControllerTest, WindowStateOnExit) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  using svc = SplitViewController;
  // Tests that normally, window will maximize on splitview ended.
  split_view_controller()->SnapWindow(window1.get(), svc::LEFT);
  split_view_controller()->SnapWindow(window2.get(), svc::RIGHT);
  split_view_controller()->EndSplitView();
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMaximized());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsMaximized());

  // Tests that if we end splitview from home launcher, the windows do not get
  // maximized.
  split_view_controller()->SnapWindow(window1.get(), svc::LEFT);
  split_view_controller()->SnapWindow(window2.get(), svc::RIGHT);
  split_view_controller()->EndSplitView(
      SplitViewController::EndReason::kHomeLauncherPressed);
  EXPECT_FALSE(WindowState::Get(window1.get())->IsMaximized());
  EXPECT_FALSE(WindowState::Get(window2.get())->IsMaximized());
}

// Test that if overview and splitview are both active at the same time,
// activiate an unsnappable window should end both overview and splitview mode.
TEST_P(SplitViewControllerTest, ActivateNonSnappableWindow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateNonSnappableWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ToggleOverview();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  wm::ActivateWindow(window3.get());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that if a snapped window has a bubble transient child, the bubble's
// bounds should always align with the snapped window's bounds.
TEST_P(SplitViewControllerTest, AdjustTransientChildBounds) {
  std::unique_ptr<views::Widget> widget(CreateTestWidget());
  aura::Window* window = widget->GetNativeWindow();
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanResize |
                          aura::client::kResizeBehaviorCanMaximize);
  split_view_controller()->SnapWindow(window, SplitViewController::LEFT);
  const gfx::Rect window_bounds = window->GetBoundsInScreen();

  // Create a bubble widget that's anchored to |widget|.
  views::Widget* bubble_widget = views::BubbleDialogDelegateView::CreateBubble(
      new TestBubbleDialogDelegateView(widget->GetContentsView()));
  aura::Window* bubble_window = bubble_widget->GetNativeWindow();
  EXPECT_TRUE(::wm::HasTransientAncestor(bubble_window, window));
  // Test that the bubble is created inside its anchor widget.
  EXPECT_TRUE(window_bounds.Contains(bubble_window->GetBoundsInScreen()));

  // Now try to manually move the bubble out of the snapped window.
  bubble_window->SetBoundsInScreen(
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::RIGHT, window),
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
  // Test that the bubble can't be moved outside of its anchor widget.
  EXPECT_TRUE(window_bounds.Contains(bubble_window->GetBoundsInScreen()));
  EndSplitView();
}

// Tests the divider closest position ratio if work area is not starts from the
// top of the display.
TEST_P(SplitViewControllerTest, DividerClosestRatioOnWorkArea) {
  UpdateDisplay("1200x800");
  // Docked magnifier will put a view port window on the top of the display.
  Shell::Get()->docked_magnifier_controller()->SetEnabled(true);

  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  ui::test::EventGenerator* generator = GetEventGenerator();
  ASSERT_EQ(OrientationLockType::kLandscapePrimary,
            test_api.GetCurrentOrientation());

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);

  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(OrientationLockType::kPortraitSecondary,
            test_api.GetCurrentOrientation());
  EXPECT_EQ(divider_closest_ratio(), 0.5f);

  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(OrientationLockType::kLandscapePrimary,
            test_api.GetCurrentOrientation());
  EXPECT_EQ(divider_closest_ratio(), 0.5f);
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  gfx::Rect workarea_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window.get());
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  // Drag the divider to one third position of the work area's width.
  generator->DragMouseTo(
      gfx::Point(workarea_bounds.width() * 0.33f, workarea_bounds.y()));
  SkipDividerSnapAnimation();
  EXPECT_EQ(divider_closest_ratio(), 0.33f);

  // Divider closest position ratio changed from one third to two thirds if
  // left/top window changes.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(OrientationLockType::kPortraitSecondary,
            test_api.GetCurrentOrientation());
  EXPECT_EQ(divider_closest_ratio(), 0.67f);

  // Divider closest position ratio is kept as one third if left/top window
  // doesn't changes.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(OrientationLockType::kPortraitPrimary,
            test_api.GetCurrentOrientation());
  EXPECT_EQ(divider_closest_ratio(), 0.33f);
}

// Tests that the divider closest position ratio is properly updated for display
// rotation after a clamshell/tablet transition that does not trigger a call to
// |SplitViewController::OnDisplayMetricsChanged|. The point here is that if
// |SplitViewController::is_previous_layout_right_side_up_| is only ever updated
// in |SplitViewController::OnDisplayMetricsChanged|, then a clamshell/tablet
// transition can leave it with a stale value which can cause broken behavior.
TEST_P(SplitViewControllerTest,
       DividerClosestRatioUpdatedForClamshellTabletTransition) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  // Set the display orientation to landscape secondary (upside down).
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  // Switch to clamshell mode.
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->SetEnabledForTest(false);
  // Set the display orientation to landscape secondary (upside down).
  Shell::Get()->display_manager()->SetDisplayRotation(
      display_id, display::Display::ROTATE_180,
      display::Display::RotationSource::ACTIVE);
  // Switch to tablet mode.
  tablet_mode_controller->SetEnabledForTest(true);
  // Enter split view.
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  // Drag the divider so that the snapped window spans only one third of the way
  // across the work area.
  ui::test::EventGenerator* generator = GetEventGenerator();
  const gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  const gfx::Rect workarea_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window.get());
  generator->DragMouseTo(
      gfx::Point(workarea_bounds.width() * 0.33f, workarea_bounds.y()));
  SkipDividerSnapAnimation();
  // Expect that the divider closest position ratio is two thirds with the
  // display upside down.
  EXPECT_EQ(divider_closest_ratio(), 0.67f);
  // Set the display orientation to landscape primary (right side up).
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  // Expect that the divider closest position ratio is updated to one third.
  EXPECT_EQ(divider_closest_ratio(), 0.33f);
}

// Test that if we snap an always on top window in splitscreen, there should be
// no crash and the window should stay always on top.
TEST_P(SplitViewControllerTest, AlwaysOnTopWindow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> always_on_top_window(CreateWindow(bounds));
  always_on_top_window->SetProperty(aura::client::kZOrderingKey,
                                    ui::ZOrderLevel::kFloatingWindow);
  std::unique_ptr<aura::Window> normal_window(CreateWindow(bounds));

  split_view_controller()->SnapWindow(always_on_top_window.get(),
                                      SplitViewController::LEFT);
  split_view_controller()->SnapWindow(normal_window.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            always_on_top_window->GetProperty(aura::client::kZOrderingKey));

  wm::ActivateWindow(always_on_top_window.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            always_on_top_window->GetProperty(aura::client::kZOrderingKey));

  wm::ActivateWindow(normal_window.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            always_on_top_window->GetProperty(aura::client::kZOrderingKey));
}

// Test that pinning a window ends split view mode.
TEST_P(SplitViewControllerTest, PinningWindowEndsSplitView) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  window_util::PinWindow(window1.get(), true);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test that split view mode is disallowed while we're in pinned mode (there is
// a pinned window).
TEST_P(SplitViewControllerTest, PinnedWindowDisallowsSplitView) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  EXPECT_TRUE(ShouldAllowSplitView());

  window_util::PinWindow(window1.get(), true);
  EXPECT_FALSE(ShouldAllowSplitView());
}

// Test that if split view ends while the divider is dragged to where a snapped
// window is sliding off the screen because it has reached minimum size, then
// the offset is cleared.
TEST_P(SplitViewControllerTest, EndSplitViewWhileResizingBeyondMinimum) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  const gfx::Rect bounds(0, 0, 300, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate =
      static_cast<aura::test::TestWindowDelegate*>(window->delegate());

  // Set the screen orientation to LANDSCAPE_PRIMARY
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);

  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window.get());
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  delegate->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.4f, display_bounds.height()));

  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  gfx::Point resize_point(display_bounds.width() * 0.33f, 0);
  split_view_controller()->Resize(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow", 1);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow",
      0);

  ASSERT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  EndSplitView();
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow", 1);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow",
      1);

  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
}

// Test if presentation time is recorded for multi window resizing
// and resizing with overview.
TEST_P(SplitViewControllerTest, ResizeTwoWindows) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  const gfx::Rect bounds(0, 0, 300, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  gfx::Point resize_point(display_bounds.width() * 0.33f, 0);
  split_view_controller()->Resize(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.MultiWindow", 1);
  split_view_controller()->Resize(
      gfx::Point(resize_point.x(), resize_point.y() + 1));
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.MultiWindow", 2);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.MultiWindow",
      0);

  split_view_controller()->EndResize(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.MultiWindow", 2);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.MultiWindow",
      1);

  ToggleOverview();

  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  split_view_controller()->Resize(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.WithOverview", 1);
  split_view_controller()->Resize(
      gfx::Point(resize_point.x(), resize_point.y() + 1));
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.WithOverview", 2);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.WithOverview",
      0);
  split_view_controller()->EndResize(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.WithOverview", 2);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.WithOverview",
      1);
}

// Test that if split view ends during the divider snap animation while a
// snapped window is sliding off the screen because it has reached minimum size,
// then the animation is ended and the window offset is cleared.
TEST_P(SplitViewControllerTest, EndSplitViewDuringDividerSnapAnimation) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  const gfx::Rect bounds(0, 0, 300, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate =
      static_cast<aura::test::TestWindowDelegate*>(window->delegate());

  // Set the screen orientation to LANDSCAPE_PRIMARY
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);

  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window.get());
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  delegate->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.4f, display_bounds.height()));

  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  gfx::Point resize_point((int)(display_bounds.width() * 0.33f) + 20, 0);
  split_view_controller()->Resize(resize_point);
  split_view_controller()->EndResize(resize_point);
  ASSERT_TRUE(IsDividerAnimating());
  ASSERT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  EndSplitView();
  EXPECT_FALSE(IsDividerAnimating());
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
}

// TestOverviewObserver which tracks how many overview items there are when
// overview mode is about to end.
class TestOverviewItemsOnOverviewModeEndObserver : public OverviewObserver {
 public:
  TestOverviewItemsOnOverviewModeEndObserver() {
    Shell::Get()->overview_controller()->AddObserver(this);
  }
  ~TestOverviewItemsOnOverviewModeEndObserver() override {
    Shell::Get()->overview_controller()->RemoveObserver(this);
  }
  void OnOverviewModeEnding(OverviewSession* overview_session) override {
    items_on_last_overview_end_ = overview_session->num_items();
  }
  int items_on_last_overview_end() const { return items_on_last_overview_end_; }

 private:
  int items_on_last_overview_end_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestOverviewItemsOnOverviewModeEndObserver);
};

TEST_P(SplitViewControllerTest, ItemsRemovedFromOverviewOnSnap) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  ASSERT_EQ(
      2u, Shell::Get()->overview_controller()->overview_session()->num_items());
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ASSERT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(
      1u, Shell::Get()->overview_controller()->overview_session()->num_items());

  // Create |observer| after splitview is entered so that it gets notified after
  // splitview does, and so will notice the changes splitview made to overview
  // on overview end.
  TestOverviewItemsOnOverviewModeEndObserver observer;
  ToggleOverview();
  EXPECT_EQ(0, observer.items_on_last_overview_end());
}

// Test that resizing ends properly if split view ends during divider dragging.
TEST_P(SplitViewControllerTest, EndSplitViewWhileDragging) {
  // Enter split view mode.
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);

  // Start resizing.
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);

  split_view_controller()->StartResize(divider_bounds.CenterPoint());

  // Verify the setup.
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  ASSERT_TRUE(split_view_controller()->is_resizing());

  gfx::Point resize_point(divider_bounds.CenterPoint());
  resize_point.Offset(100, 0);

  split_view_controller()->Resize(resize_point);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow", 1);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow",
      0);

  // End split view and check that resizing has ended properly.
  split_view_controller()->EndSplitView();
  EXPECT_FALSE(split_view_controller()->is_resizing());
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow", 1);
  histograms().ExpectTotalCount(
      "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow",
      1);
}

// Test the tab-dragging related functionalities in tablet mode. Tab(s) can be
// dragged out of a window and then put in split view mode or merge into another
// window.
class SplitViewTabDraggingTest : public SplitViewControllerTest {
 public:
  SplitViewTabDraggingTest() = default;
  ~SplitViewTabDraggingTest() override = default;

 protected:
  aura::Window* CreateWindowWithType(
      const gfx::Rect& bounds,
      AppType app_type,
      aura::client::WindowType window_type = aura::client::WINDOW_TYPE_NORMAL) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        new SplitViewTestWindowDelegate, window_type, -1, bounds);
    window->SetProperty(aura::client::kAppType, static_cast<int>(app_type));
    WindowState::Get(window)->Maximize();
    return window;
  }

  // Starts tab dragging on |dragged_window|. |source_window| indicates which
  // window the drag originates from. Returns the newly created WindowResizer
  // for the |dragged_window|.
  std::unique_ptr<WindowResizer> StartDrag(aura::Window* dragged_window,
                                           aura::Window* source_window) {
    // Drag operation activates the window first, then activates the dragged
    // window.  Emulate this behavior.
    wm::ActivateWindow(source_window);
    SetIsInTabDragging(dragged_window, /*is_dragging=*/true, source_window);
    wm::ActivateWindow(dragged_window);
    std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(
        dragged_window, dragged_window->bounds().origin(), HTCAPTION);
    GetBrowserWindowDragController(resizer.get())
        ->drag_delegate_for_testing()
        ->set_drag_start_deadline_for_testing(base::Time::Now());
    return resizer;
  }

  // Drags the window to |end_position|.
  void DragWindowTo(WindowResizer* resizer, const gfx::Point& end_position) {
    ASSERT_TRUE(resizer);
    resizer->Drag(gfx::PointF(end_position), 0);
  }

  // Drags the window with offest (delta_x, delta_y) to its initial position.
  void DragWindowWithOffset(WindowResizer* resizer, int delta_x, int delta_y) {
    ASSERT_TRUE(resizer);
    gfx::PointF location = resizer->GetInitialLocation();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    resizer->Drag(location, 0);
  }

  // Ends the drag. |resizer| will be deleted after exiting this function.
  void CompleteDrag(std::unique_ptr<WindowResizer> resizer) {
    ASSERT_TRUE(resizer.get());
    resizer->CompleteDrag();
    SetIsInTabDragging(resizer->GetTarget(), /*is_dragging=*/false);
  }

  // Fling to end the drag. |resizer| will be deleted after exiting this
  // function.
  void Fling(std::unique_ptr<WindowResizer> resizer, float velocity_y) {
    ASSERT_TRUE(resizer.get());
    aura::Window* target_window = resizer->GetTarget();
    base::TimeTicks timestamp = base::TimeTicks::Now();
    ui::GestureEventDetails details =
        ui::GestureEventDetails(ui::ET_SCROLL_FLING_START, 0.f, velocity_y);
    ui::GestureEvent event = ui::GestureEvent(
        target_window->bounds().origin().x(),
        target_window->bounds().origin().y(), ui::EF_NONE, timestamp, details);
    ui::Event::DispatcherApi(&event).set_target(target_window);
    resizer->FlingOrSwipe(&event);
    SetIsInTabDragging(resizer->GetTarget(), /*is_dragging=*/false);
  }

  std::unique_ptr<WindowResizer> CreateResizerForTest(
      aura::Window* window,
      const gfx::Point& point_in_parent,
      int window_component,
      ::wm::WindowMoveSource source = ::wm::WINDOW_MOVE_SOURCE_TOUCH) {
    return CreateWindowResizer(window, gfx::PointF(point_in_parent),
                               window_component, source);
  }

  // Sets if |dragged_window| is currently in tab-dragging process.
  // |source_window| is the window that the drag originates from. This method is
  // used to simulate the start/stop of a window's tab-dragging by setting the
  // two window properties, which are usually set in TabDragController::
  // UpdateTabDraggingInfo() function.
  void SetIsInTabDragging(aura::Window* dragged_window,
                          bool is_dragging,
                          aura::Window* source_window = nullptr) {
    if (!is_dragging) {
      dragged_window->ClearProperty(kIsDraggingTabsKey);
      dragged_window->ClearProperty(kTabDraggingSourceWindowKey);
    } else {
      dragged_window->SetProperty(kIsDraggingTabsKey, is_dragging);
      if (source_window != dragged_window)
        dragged_window->SetProperty(kTabDraggingSourceWindowKey, source_window);
    }
  }

  TabletModeWindowResizer* GetBrowserWindowDragController(
      WindowResizer* resizer) {
    WindowResizer* real_window_resizer;
    // TODO(xdai): This piece of codes seems knowing too much impl details about
    // WindowResizer. Revisit the logic here later to see if there is anything
    // we can do to simplify the logic and hide impl details.
    real_window_resizer = static_cast<DragWindowResizer*>(resizer)
                              ->next_window_resizer_for_testing();
    return static_cast<TabletModeWindowResizer*>(real_window_resizer);
  }

  SplitViewDragIndicators::WindowDraggingState GetWindowDraggingState(
      WindowResizer* resizer) {
    return GetBrowserWindowDragController(resizer)
        ->drag_delegate_for_testing()
        ->split_view_drag_indicators_for_testing()
        ->current_window_dragging_state();
  }

  int GetIndicatorsThreshold(aura::Window* dragged_window) {
    static const float kIndicatorsThresholdRatio = 0.1f;
    const gfx::Rect work_area_bounds =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(dragged_window)
            .work_area();
    return work_area_bounds.y() +
           work_area_bounds.height() * kIndicatorsThresholdRatio;
  }

  gfx::Rect GetDropTargetBoundsDuringDrag(aura::Window* window) const {
    OverviewSession* overview_session =
        Shell::Get()->overview_controller()->overview_session();
    DCHECK(overview_session);
    OverviewGrid* current_grid =
        overview_session->GetGridWithRootWindow(window->GetRootWindow());
    DCHECK(current_grid);

    OverviewItem* overview_item = current_grid->GetDropTarget();
    return gfx::ToEnclosedRect(overview_item->GetTransformedBounds());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SplitViewTabDraggingTest);
};

// Test that in tablet mode, we only allow dragging on browser or chrome app
// window's caption area.
TEST_P(SplitViewTabDraggingTest, OnlyAllowDraggingOnBrowserOrChromeAppWindow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::CHROME_APP));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window4(
      CreateWindowWithType(bounds, AppType::ARC_APP));

  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window1.get(), gfx::Point(), HTCAPTION);
  EXPECT_TRUE(resizer.get());
  resizer->CompleteDrag();
  resizer.reset();

  resizer = CreateResizerForTest(window2.get(), gfx::Point(), HTCAPTION);
  EXPECT_TRUE(resizer.get());
  resizer->CompleteDrag();
  resizer.reset();

  resizer = CreateResizerForTest(window3.get(), gfx::Point(), HTCAPTION);
  EXPECT_FALSE(resizer.get());

  resizer = CreateResizerForTest(window4.get(), gfx::Point(), HTCAPTION);
  EXPECT_FALSE(resizer.get());
}

// Test that in tablet mode, we only allow dragging that happens on window
// caption or top area.
TEST_P(SplitViewTabDraggingTest, OnlyAllowDraggingOnCaptionOrTopArea) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(
      CreateWindowWithType(bounds, AppType::BROWSER));

  // Only dragging on HTCAPTION or HTTOP area is allowed.
  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window.get(), gfx::Point(), HTLEFT);
  EXPECT_FALSE(resizer.get());
  resizer = CreateResizerForTest(window.get(), gfx::Point(), HTRIGHT);
  EXPECT_FALSE(resizer.get());
  resizer = CreateResizerForTest(window.get(), gfx::Point(), HTCAPTION);
  EXPECT_TRUE(resizer.get());
  resizer->CompleteDrag();
  resizer.reset();
  resizer = CreateResizerForTest(window.get(), gfx::Point(), HTTOP);
  EXPECT_TRUE(resizer.get());
  resizer->CompleteDrag();
  resizer.reset();

  // No matter if we're in tab-dragging process, as long as the drag happens on
  // the caption or top area, it should be able to drag the window.
  SetIsInTabDragging(window.get(), /*is_dragging=*/true);
  resizer = CreateResizerForTest(window.get(), gfx::Point(), HTCAPTION);
  EXPECT_TRUE(resizer.get());
  resizer->CompleteDrag();
  resizer.reset();

  SetIsInTabDragging(window.get(), /*is_dragging=*/false);
  resizer = CreateResizerForTest(window.get(), gfx::Point(), HTCAPTION);
  EXPECT_TRUE(resizer.get());
  resizer->CompleteDrag();
  resizer.reset();
}

// Test that in tablet mode, if the dragging is from mouse event, the mouse
// cursor should be properly locked.
TEST_P(SplitViewTabDraggingTest, LockCursor) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  SetIsInTabDragging(window.get(), /*is_dragging=*/true);
  EXPECT_FALSE(Shell::Get()->cursor_manager()->IsCursorLocked());

  std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(
      window.get(), gfx::Point(), HTCAPTION, ::wm::WINDOW_MOVE_SOURCE_MOUSE);
  ASSERT_TRUE(resizer.get());
  EXPECT_TRUE(Shell::Get()->cursor_manager()->IsCursorLocked());

  resizer->CompleteDrag();
  resizer.reset();
  EXPECT_FALSE(Shell::Get()->cursor_manager()->IsCursorLocked());
}

// Test that in tablet mode, if a window is in tab-dragging process, its
// backdrop is disabled during dragging process.
TEST_P(SplitViewTabDraggingTest, NoBackDropDuringDragging) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  WindowBackdrop* window_backdrop = WindowBackdrop::Get(window.get());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  EXPECT_EQ(window_backdrop->type(), WindowBackdrop::BackdropType::kOpaque);

  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window.get(), window.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_TRUE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  EXPECT_EQ(window_backdrop->type(), WindowBackdrop::BackdropType::kOpaque);

  resizer->Drag(gfx::PointF(), 0);
  EXPECT_TRUE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  EXPECT_EQ(window_backdrop->type(), WindowBackdrop::BackdropType::kOpaque);

  resizer->CompleteDrag();
  EXPECT_FALSE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  EXPECT_EQ(window_backdrop->type(), WindowBackdrop::BackdropType::kOpaque);
}

// Test that in tablet mode, the window that is in tab-dragging process should
// not be shown in overview mode.
TEST_P(SplitViewTabDraggingTest, DoNotShowDraggedWindowInOverview) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));

  ToggleOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  EXPECT_TRUE(overview_session->IsWindowInOverview(window1.get()));
  EXPECT_TRUE(overview_session->IsWindowInOverview(window2.get()));
  ToggleOverview();

  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());

  // Since the source window is the dragged window, the overview should have
  // been opened.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  overview_session = Shell::Get()->overview_controller()->overview_session();
  EXPECT_FALSE(overview_session->IsWindowInOverview(window1.get()));
  EXPECT_TRUE(overview_session->IsWindowInOverview(window2.get()));

  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Test that if a window is in tab-dragging process, the split divider is placed
// below the current dragged window.
TEST_P(SplitViewTabDraggingTest, DividerIsBelowDraggedWindow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  views::Widget* split_divider_widget =
      split_view_controller()->split_view_divider()->divider_widget();
  EXPECT_NE(ui::ZOrderLevel::kNormal, split_divider_widget->GetZOrderLevel());

  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_EQ(ui::ZOrderLevel::kNormal, split_divider_widget->GetZOrderLevel());

  resizer->Drag(gfx::PointF(), 0);
  EXPECT_EQ(ui::ZOrderLevel::kNormal, split_divider_widget->GetZOrderLevel());

  CompleteDrag(std::move(resizer));
  EXPECT_NE(ui::ZOrderLevel::kNormal, split_divider_widget->GetZOrderLevel());
}

// Test the functionalities that are related to dragging a maximized window's
// tabs. See the expected behaviors described in go/tab-dragging-in-tablet-mode.
TEST_P(SplitViewTabDraggingTest, DragMaximizedWindow) {
  UpdateDisplay("600x600");
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  WindowState::Get(window1.get())->Maximize();
  WindowState::Get(window2.get())->Maximize();
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMaximized());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsMaximized());

  // 1. If the dragged window is the source window:
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  // Overview should have been opened because the dragged window is the source
  // window.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // 1.a. Drag the window to move a small amount of distance will maximize the
  // window again.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            GetWindowDraggingState(resizer.get()));
  // Drag the window past the indicators threshold to show the indicators.
  DragWindowTo(resizer.get(),
               gfx::Point(200, GetIndicatorsThreshold(window1.get())));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromTop,
            GetWindowDraggingState(resizer.get()));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMaximized());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsMaximized());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  // 1.b. Drag the window long enough (pass one fourth of the screen vertical
  // height) to snap the window to splitscreen.
  resizer = StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  DragWindowTo(resizer.get(), gfx::Point(0, 300));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
            GetWindowDraggingState(resizer.get()));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Maximize the snapped window to end split view mode and overview mode.
  WindowState::Get(window1.get())->Maximize();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  // 2. If the dragged window is not the source window:
  resizer = StartDrag(window1.get(), window2.get());
  ASSERT_TRUE(resizer.get());
  // Overview is not opened for this case.
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  // When the drag starts, the source window's bounds are the same with the
  // work area's bounds.
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window1.get());
  const gfx::Rect work_area_bounds = display.work_area();
  EXPECT_EQ(window2->GetBoundsInScreen(), work_area_bounds);
  EXPECT_TRUE(window1->GetProperty(kCanAttachToAnotherWindowKey));

  // 2.a. Drag the window a small amount of distance and release will maximize
  // the window.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            GetWindowDraggingState(resizer.get()));
  // Drag the window past the indicators threshold to show the indicators.
  DragWindowTo(resizer.get(),
               gfx::Point(200, GetIndicatorsThreshold(window1.get())));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromTop,
            GetWindowDraggingState(resizer.get()));
  // The source window should also have been scaled.
  EXPECT_NE(window2->GetBoundsInScreen(), work_area_bounds);
  EXPECT_FALSE(window1->GetProperty(kCanAttachToAnotherWindowKey));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMaximized());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsMaximized());
  // The source window should have restored its bounds.
  EXPECT_EQ(window2->GetBoundsInScreen(), work_area_bounds);
  EXPECT_TRUE(window1->GetProperty(kCanAttachToAnotherWindowKey));

  // 2.b. Drag the window long enough to snap the window. The source window will
  // snap to the other side of the splitscreen.
  resizer = StartDrag(window1.get(), window2.get());
  ASSERT_TRUE(resizer.get());
  DragWindowTo(resizer.get(), gfx::Point(600, 300));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapRight,
            GetWindowDraggingState(resizer.get()));
  // The source window's bounds should be the same as the left snapped window
  // bounds as it's to be snapped to LEFT.
  EXPECT_EQ(window2->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                SplitViewController::LEFT, window2.get()));
  EXPECT_FALSE(window1->GetProperty(kCanAttachToAnotherWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(0, 300));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
            GetWindowDraggingState(resizer.get()));
  // The source window's bounds should be the same as the right snapped window
  // bounds as it's to be snapped to RIGHT.
  EXPECT_EQ(window2->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                SplitViewController::RIGHT, window2.get()));
  EXPECT_FALSE(window1->GetProperty(kCanAttachToAnotherWindowKey));

  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(window1->GetProperty(kCanAttachToAnotherWindowKey));
  EXPECT_EQ(window1.get(), window_util::GetActiveWindow());

  EndSplitView();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // 3. If the dragged window is destroyed during dragging (may happen due to
  // all its tabs are attached into another window), nothing changes.
  resizer = StartDrag(window1.get(), window2.get());
  ASSERT_TRUE(resizer.get());
  resizer->Drag(gfx::PointF(0, 300), 0);
  resizer->CompleteDrag();
  resizer.reset();
  window1.reset();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsMaximized());

  // 4. If the dragged window can't be snapped, then the source window should
  // not be put to the snapped position during drag.
  const gfx::Rect display_bounds = display.bounds();
  window1 = std::unique_ptr<aura::Window>(
      CreateWindowWithType(bounds, AppType::BROWSER));
  aura::test::TestWindowDelegate* delegate =
      static_cast<aura::test::TestWindowDelegate*>(window1->delegate());
  delegate->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.67f, display_bounds.height()));
  EXPECT_FALSE(split_view_controller()->CanSnapWindow(window1.get()));
  resizer = StartDrag(window1.get(), window2.get());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  DragWindowTo(resizer.get(),
               gfx::Point(0, GetIndicatorsThreshold(window1.get()) + 10));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromTop,
            GetWindowDraggingState(resizer.get()));
  // The souce window should has been scaled but not put to the right snapped
  // window's position.
  EXPECT_NE(window2->GetBoundsInScreen(), work_area_bounds);
  EXPECT_NE(window2->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                SplitViewController::RIGHT, window2.get()));
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test the functionalities that are related to dragging a snapped window in
// splitscreen. There are always two snapped window when the drag starts (i.e.,
// the overview mode is not active). See the expected behaviors described in
// go/tab-dragging-in-tablet-mode.
TEST_P(SplitViewTabDraggingTest, DragSnappedWindow) {
  UpdateDisplay("600x600");
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithType(bounds, AppType::BROWSER));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());

  // 1. If the dragged window is the source window:
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            GetWindowDraggingState(resizer.get()));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kRightSnapped);
  // In this case overview grid will be opened, containing |window3|.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  EXPECT_FALSE(overview_session->IsWindowInOverview(window1.get()));
  EXPECT_FALSE(overview_session->IsWindowInOverview(window2.get()));
  EXPECT_TRUE(overview_session->IsWindowInOverview(window3.get()));
  // Drag to enter doesn't trigger animation.
  CheckOverviewEnterExitHistogram("EnterInSplitViewByDrag", {0, 0}, {0, 0});

  // 1.a. If the window is only dragged for a small distance, the window will
  // be put back to its original position. Overview mode will be ended.
  DragWindowWithOffset(resizer.get(), 10, 10);
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  CheckOverviewEnterExitHistogram("ExitInSplitViewByDrag", {0, 0}, {0, 1});

  // 1.b. If the window is dragged long enough, it can replace the other split
  // window.
  resizer = StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  DragWindowTo(resizer.get(), gfx::Point(600, 300));
  // Preview window shows up on overview side of screen.
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapRight,
            GetWindowDraggingState(resizer.get()));
  CheckOverviewEnterExitHistogram("EnterInSplitViewByDrag2", {0, 0}, {0, 1});

  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kRightSnapped);
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  overview_session = Shell::Get()->overview_controller()->overview_session();
  EXPECT_FALSE(overview_session->IsWindowInOverview(window1.get()));
  EXPECT_TRUE(overview_session->IsWindowInOverview(window2.get()));
  EXPECT_TRUE(overview_session->IsWindowInOverview(window3.get()));
  // Still in overview.
  CheckOverviewEnterExitHistogram("DoNotExitInSplitViewByDrag", {0, 0}, {0, 1});
  // Snap |window2| again to test 1.c.
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  CheckOverviewEnterExitHistogram("ExitInSplitViewBySnap", {0, 0}, {0, 2});

  // 1.c. If the dragged window is destroyed during dragging (may happen due to
  // all its tabs are attached into another window), nothing changes.
  resizer = StartDrag(window1.get(), window1.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  ASSERT_TRUE(resizer.get());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  CheckOverviewEnterExitHistogram("EnterInSplitViewByDrag3", {0, 0}, {0, 2});
  resizer->Drag(gfx::PointF(100, 100), 0);
  resizer->CompleteDrag();
  resizer.reset();
  window1.reset();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  // Still in overview.
  CheckOverviewEnterExitHistogram("DoNotExitInSplitViewByDrag3", {0, 0},
                                  {0, 2});

  // Recreate |window1| and snap it to test the following senarioes.
  window1.reset(CreateWindowWithType(bounds, AppType::BROWSER));
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  CheckOverviewEnterExitHistogram("ExitInSplitViewBySnap2", {0, 0}, {0, 3});

  // 2. If the dragged window is not the source window:
  // In this case, |window3| can be regarded as a window that originates from
  // |window2|.
  resizer = StartDrag(window3.get(), window2.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            GetWindowDraggingState(resizer.get()));
  EXPECT_EQ(window2->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                SplitViewController::LEFT, window2.get()));
  // Not in overview.
  CheckOverviewEnterExitHistogram("DoNotEnterInSplitViewByDrag", {0, 0},
                                  {0, 3});

  // 2.a. If the window is only dragged for a small amount of distance, it will
  // replace the same side of the split window that it originates from.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            GetWindowDraggingState(resizer.get()));
  // Drag the window past the indicators threshold to "show the indicators" (see
  // the comment about this whole unit test).
  DragWindowTo(resizer.get(),
               gfx::Point(200, GetIndicatorsThreshold(window1.get())));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromTop,
            GetWindowDraggingState(resizer.get()));
  // The source window's bounds should remain the same.
  EXPECT_EQ(window2->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                SplitViewController::LEFT, window2.get()));
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window3.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  CheckOverviewEnterExitHistogram("DoNotEnterInSplitViewByDrag2", {0, 0},
                                  {0, 3});

  // 2.b. If the window is dragged long enough, it can replace the other side of
  // the split window.
  resizer = StartDrag(window2.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window3.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  CheckOverviewEnterExitHistogram("DoNotEnterInSplitViewByDrag3", {0, 0},
                                  {0, 3});

  DragWindowTo(resizer.get(), gfx::Point(0, 300));
  // Preview window shows up on overview side of screen.
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
            GetWindowDraggingState(resizer.get()));
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  CheckOverviewEnterExitHistogram("DoNotEnterInSplitViewByDrag4", {0, 0},
                                  {0, 3});

  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Test the functionalities that are related to dragging a snapped window while
// overview grid is open on the other side of the screen. See the expected
// behaviors described in go/tab-dragging-in-tablet-mode.
TEST_P(SplitViewTabDraggingTest, DragSnappedWindowWhileOverviewOpen) {
  UpdateDisplay("600x600");
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithType(bounds, AppType::BROWSER));

  // Prepare the testing senario:
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  ToggleOverview();
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  CheckOverviewEnterExitHistogram("EnterInSplitView", {0, 1}, {0, 0});

  // 1. If the dragged window is the source window:
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  // Overivew mode is still active, but split view mode is ended due to dragging
  // the only snapped window.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // 1.a. If the window is only dragged for a small amount of distance
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            GetWindowDraggingState(resizer.get()));
  // Drag the window past the indicators threshold should "show the indicators"
  // (see the comment on SplitViewTabDraggingTest.DragSnappedWindow).
  DragWindowTo(resizer.get(),
               gfx::Point(200, GetIndicatorsThreshold(window1.get())));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromTop,
            GetWindowDraggingState(resizer.get()));
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMaximized());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  // It exits SplitView during drag, so exit animation is performed in tablet
  // mode.
  CheckOverviewEnterExitHistogram("ExitInSplitViewToTablet", {0, 1}, {1, 0});

  // 1.b. If the window is dragged long enough, it can be snappped again.
  // Prepare the testing senario first.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  ToggleOverview();
  CheckOverviewEnterExitHistogram("EnterInSplitView2", {0, 2}, {1, 0});

  resizer = StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  CheckOverviewEnterExitHistogram("DoNotExitInSplitView2", {0, 2}, {1, 0});

  DragWindowTo(resizer.get(), gfx::Point(0, 300));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
            GetWindowDraggingState(resizer.get()));
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  EXPECT_TRUE(overview_session->IsWindowInOverview(window2.get()));
  EXPECT_TRUE(overview_session->IsWindowInOverview(window3.get()));
  CheckOverviewEnterExitHistogram("DoNotExitInSplitView3", {0, 2}, {1, 0});

  // 2. If the dragged window is not the source window:
  // Prepare the testing senario first. Remove |window2| from overview first
  // before tab-dragging.
  OverviewGrid* current_grid =
      overview_session->GetGridWithRootWindow(window2->GetRootWindow());
  ASSERT_TRUE(current_grid);
  overview_session->RemoveItem(
      current_grid->GetOverviewItemContaining(window2.get()));

  resizer = StartDrag(window2.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            GetWindowDraggingState(resizer.get()));
  // Drag a samll amount of distance.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            GetWindowDraggingState(resizer.get()));
  // Drag the window past the indicators threshold to "show the indicators" (see
  // the comment on SplitViewTabDraggingTest.DragSnappedWindow).
  DragWindowTo(resizer.get(),
               gfx::Point(200, GetIndicatorsThreshold(window1.get())));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromTop,
            GetWindowDraggingState(resizer.get()));
  // The source window still remains the same bounds.
  EXPECT_EQ(window1->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                SplitViewController::LEFT, window1.get()));
  CheckOverviewEnterExitHistogram("DoNotExitInSplitView4", {0, 2}, {1, 0});

  // 2.a. The dragged window can replace the only snapped window in the split
  // screen. After that, the old snapped window will be put back in overview.
  DragWindowTo(resizer.get(), gfx::Point(0, 500));
  // Preview window shows up on overview side of screen.
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
            GetWindowDraggingState(resizer.get()));
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  overview_session = Shell::Get()->overview_controller()->overview_session();
  EXPECT_TRUE(overview_session->IsWindowInOverview(window1.get()));
  EXPECT_TRUE(overview_session->IsWindowInOverview(window3.get()));
  CheckOverviewEnterExitHistogram("DoNotExitInSplitView5", {0, 2}, {1, 0});

  // 2.b. The dragged window can snap to the other side of the splitscreen,
  // causing overview mode to end.
  // Remove |window1| from overview first before tab dragging.
  overview_session->RemoveItem(
      current_grid->GetOverviewItemContaining(window1.get()));
  resizer = StartDrag(window1.get(), window2.get());
  ASSERT_TRUE(resizer.get());
  DragWindowTo(resizer.get(), gfx::Point(600, 500));
  // Preview window shows up on overview side of screen.
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapRight,
            GetWindowDraggingState(resizer.get()));
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  CheckOverviewEnterExitHistogram("ExitInSplitView", {0, 2}, {1, 1});
}

// Test that if a window is in tab-dragging process when overview is open, the
// new window item widget shows up when the drag starts, and is destroyed after
// the drag ends.
TEST_P(SplitViewTabDraggingTest, ShowNewWindowItemWhenDragStarts) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithType(bounds, AppType::BROWSER));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  // Now drags |window1|.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  // Overview should have been opened.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kRightSnapped);

  // Test that the new window item widget shows up as the first one of the
  // windows in the grid.
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  OverviewGrid* current_grid =
      overview_session->GetGridWithRootWindow(window1->GetRootWindow());
  ASSERT_TRUE(current_grid);
  views::Widget* drop_target_widget = current_grid->drop_target_widget();
  EXPECT_TRUE(drop_target_widget);

  OverviewItem* drop_target = current_grid->GetOverviewItemContaining(
      drop_target_widget->GetNativeWindow());
  ASSERT_TRUE(drop_target);
  EXPECT_EQ(drop_target, current_grid->window_list().front().get());
  const gfx::Rect drop_target_bounds =
      gfx::ToEnclosingRect(drop_target->target_bounds());
  // We want to drag onto the drop target, but not too close to the edge of the
  // screen, as we do not want to snap the window.
  DragWindowTo(resizer.get(), gfx::Point(drop_target_bounds.right() - 2,
                                         drop_target_bounds.CenterPoint().y()));
  CompleteDrag(std::move(resizer));

  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kRightSnapped);
  // Test that the dragged window has been added to the overview mode, and it is
  // added at the front of the grid.
  EXPECT_EQ(current_grid->window_list().size(), 2u);
  OverviewItem* first_overview_item =
      current_grid->GetOverviewItemContaining(window1.get());
  EXPECT_EQ(first_overview_item, current_grid->window_list().front().get());
  EXPECT_TRUE(overview_session->IsWindowInOverview(window1.get()));
  EXPECT_TRUE(overview_session->IsWindowInOverview(window3.get()));
  // Test that the new window item widget has been destroyed.
  EXPECT_FALSE(current_grid->drop_target_widget());
}

// Tests that if overview is ended because of releasing the dragged window, we
// should not do animation when exiting overview.
TEST_P(SplitViewTabDraggingTest, OverviewExitAnimationTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<OverviewStatesObserver> overview_observer =
      std::make_unique<OverviewStatesObserver>(window1->GetRootWindow());

  // 1) If dragging a maximized window:
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  // Overview should have been opened because the dragged window is the source
  // window.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  // The value should be properly initialized.
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());

  // Now release the dragged window. There should be no animation when exiting
  // overview.
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_FALSE(overview_observer->overview_animate_when_exiting());

  // 2) If dragging a snapped window:
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  overview_observer.reset(new OverviewStatesObserver(window1->GetRootWindow()));
  resizer = StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  // Overview should have been opened behind the dragged window.
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  // Split view should still be active.
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kRightSnapped);
  // The value should be properly initialized.
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());

  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_FALSE(overview_observer->overview_animate_when_exiting());
}

// Checks the drag indicators window dragging state for dragging from the top.
TEST_P(SplitViewTabDraggingTest, DragIndicatorsInPortraitOrientationTest) {
  UpdateDisplay("800x600");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  ASSERT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapePrimary);

  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  WindowState::Get(window.get())->Maximize();
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window.get(), window.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  // Drag the window past the indicators threshold to show the indicators.
  DragWindowTo(resizer.get(),
               gfx::Point(200, GetIndicatorsThreshold(window.get())));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromTop,
            GetWindowDraggingState(resizer.get()));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  // Rotate the screen by 270 degree to portrait primary orientation.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitPrimary);
  resizer = StartDrag(window.get(), window.get());
  ASSERT_TRUE(resizer.get());
  // Drag the window past the indicators threshold to show the indicators.
  DragWindowTo(resizer.get(),
               gfx::Point(200, GetIndicatorsThreshold(window.get())));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromTop,
            GetWindowDraggingState(resizer.get()));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());
}

// Tests that if dragging a window into the preview split area, overview bounds
// should be adjusted accordingly.
TEST_P(SplitViewTabDraggingTest, AdjustOverviewBoundsDuringDragging) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithType(bounds, AppType::BROWSER));

  OverviewController* selector_controller = Shell::Get()->overview_controller();
  EXPECT_FALSE(selector_controller->InOverviewSession());

  // Start dragging |window1|.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  // Overview should have been opened.
  EXPECT_TRUE(selector_controller->InOverviewSession());

  // Test that the drop target shows up as the first item in overview.
  OverviewGrid* current_grid =
      selector_controller->overview_session()->GetGridWithRootWindow(
          window1->GetRootWindow());
  EXPECT_TRUE(current_grid->GetDropTarget());
  const gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  const gfx::Rect expected_grid_bounds =
      ShrinkBoundsByHotseatInset(work_area_bounds);
  EXPECT_EQ(expected_grid_bounds, current_grid->bounds());

  auto target_opacity = [](views::Widget* widget) {
    return widget->GetNativeWindow()->layer()->GetTargetOpacity();
  };
  // The drop target should be visible.
  views::Widget* drop_target_widget = current_grid->drop_target_widget();
  EXPECT_TRUE(drop_target_widget);
  // Drop target's bounds has been set when added it into overview, which is not
  // equals to the window's bounds.
  EXPECT_EQ(drop_target_widget->GetNativeWindow()->bounds(),
            GetDropTargetBoundsDuringDrag(window1.get()));
  EXPECT_NE(drop_target_widget->GetNativeWindow()->bounds(), window1->bounds());
  EXPECT_EQ(1.f, target_opacity(drop_target_widget));

  // Now drag |window1| to the left preview split area.
  DragWindowTo(resizer.get(),
               gfx::Point(0, work_area_bounds.CenterPoint().y()));
  EXPECT_EQ(current_grid->bounds(),
            ShrinkBoundsByHotseatInset(
                split_view_controller()->GetSnappedWindowBoundsInScreen(
                    SplitViewController::RIGHT,
                    /*window_for_minimum_size=*/nullptr)));
  EXPECT_EQ(0.f, target_opacity(drop_target_widget));

  // Drag it to middle.
  DragWindowTo(resizer.get(), work_area_bounds.CenterPoint());
  EXPECT_EQ(expected_grid_bounds, current_grid->bounds());
  EXPECT_EQ(1.f, target_opacity(drop_target_widget));

  // Drag |window1| to the right preview split area.
  DragWindowTo(resizer.get(), gfx::Point(work_area_bounds.right(),
                                         work_area_bounds.CenterPoint().y()));
  EXPECT_EQ(
      current_grid->bounds(),
      ShrinkBoundsByHotseatInset(
          split_view_controller()->GetSnappedWindowBoundsInScreen(
              SplitViewController::LEFT, /*window_for_minimum_size=*/nullptr)));
  EXPECT_EQ(0.f, target_opacity(drop_target_widget));

  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kRightSnapped);
  EXPECT_TRUE(selector_controller->InOverviewSession());

  // Snap another window should end overview.
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_FALSE(selector_controller->InOverviewSession());

  // Now drag |window1| again. Overview and splitview should be both active at
  // the same time during dragging. Since one window is snapped, the hotseat
  // should not be visible and should not affect the grid bounds.
  resizer = StartDrag(window1.get(), window1.get());
  EXPECT_TRUE(selector_controller->InOverviewSession());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);

  current_grid = selector_controller->overview_session()->GetGridWithRootWindow(
      window1->GetRootWindow());
  // The drop target should be visible.
  drop_target_widget = current_grid->drop_target_widget();
  EXPECT_TRUE(drop_target_widget);
  EXPECT_EQ(1.f, target_opacity(drop_target_widget));
  EXPECT_EQ(drop_target_widget->GetNativeWindow()->bounds(),
            GetDropTargetBoundsDuringDrag(window1.get()));
  EXPECT_NE(drop_target_widget->GetNativeWindow()->bounds(), window1->bounds());
  EXPECT_EQ(
      current_grid->bounds(),
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::RIGHT, /*window_for_minimum_size=*/nullptr));

  // Drag |window1| to the right preview split area.
  DragWindowTo(resizer.get(), gfx::Point(work_area_bounds.right(),
                                         work_area_bounds.CenterPoint().y()));
  // Overview bounds stays the same.
  EXPECT_EQ(
      current_grid->bounds(),
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::RIGHT, /*window_for_minimum_size=*/nullptr));
  EXPECT_EQ(0.f, target_opacity(drop_target_widget));

  // Drag |window1| to the left preview split area.
  DragWindowTo(resizer.get(),
               gfx::Point(0, work_area_bounds.CenterPoint().y()));
  EXPECT_EQ(
      current_grid->bounds(),
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::RIGHT, /*window_for_minimum_size=*/nullptr));
  EXPECT_EQ(0.f, target_opacity(drop_target_widget));

  CompleteDrag(std::move(resizer));

  // |window1| should now snap to left. |window2| is put back in overview.
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_TRUE(selector_controller->InOverviewSession());
  EXPECT_TRUE(selector_controller->overview_session()->IsWindowInOverview(
      window2.get()));

  // Now drag |window1| again.
  resizer = StartDrag(window1.get(), window1.get());
  // Splitview should end now, but overview should still active.
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(selector_controller->InOverviewSession());
  // The size of drop target should still not be the same as the dragged
  // window's size.
  current_grid = selector_controller->overview_session()->GetGridWithRootWindow(
      window1->GetRootWindow());
  drop_target_widget = current_grid->drop_target_widget();
  EXPECT_TRUE(drop_target_widget);
  EXPECT_EQ(1.f, target_opacity(drop_target_widget));
  EXPECT_EQ(drop_target_widget->GetNativeWindow()->bounds(),
            GetDropTargetBoundsDuringDrag(window1.get()));
  EXPECT_NE(drop_target_widget->GetNativeWindow()->bounds(), window1->bounds());
  CompleteDrag(std::move(resizer));
}

// Tests that a dragged window's bounds should be updated before dropping onto
// the drop target to add into overview.
TEST_P(SplitViewTabDraggingTest, WindowBoundsUpdatedBeforeAddingToOverview) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  wm::ActivateWindow(window1.get());
  gfx::Rect tablet_mode_bounds = window1->bounds();
  EXPECT_NE(bounds, tablet_mode_bounds);

  // Drag |window1|. Overview should open behind the dragged window.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Change the |window1|'s bounds to simulate what might happen in reality.
  window1->SetBounds(bounds);
  EXPECT_EQ(bounds, window1->bounds());

  // Drop |window1| to the drop target in overview.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  OverviewSession* overview_session = overview_controller->overview_session();
  OverviewGrid* current_grid =
      overview_session->GetGridWithRootWindow(window1->GetRootWindow());
  ASSERT_TRUE(current_grid);
  EXPECT_EQ(1u, current_grid->window_list().size());

  OverviewItem* overview_item = current_grid->GetDropTarget();
  ASSERT_TRUE(overview_item);
  gfx::Rect drop_target_bounds =
      gfx::ToEnclosingRect(overview_item->target_bounds());
  DragWindowTo(resizer.get(), drop_target_bounds.CenterPoint());

  CompleteDrag(std::move(resizer));
  // |window1| should have been merged into overview.
  EXPECT_EQ(current_grid->window_list().size(), 1u);
  EXPECT_TRUE(overview_session->IsWindowInOverview(window1.get()));
  // |window1|'s bounds should have been updated to its tablet mode bounds.
  EXPECT_EQ(tablet_mode_bounds, window1->bounds());
  overview_item = current_grid->window_list().front().get();
  // The new overview item's bounds should be the same during drag and after
  // drag.
  EXPECT_EQ(drop_target_bounds,
            gfx::ToEnclosingRect(overview_item->target_bounds()));
  ToggleOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Drag |window1|. Overview should open behind the dragged window.
  resizer = StartDrag(window1.get(), window1.get());
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Change the |window1|'s bounds to simulate what might happen in reality.
  window1->SetBounds(bounds);
  EXPECT_EQ(bounds, window1->bounds());

  // Drag the window to right bottom outside the drop target, the window's
  // bounds should also be updated before being dropped into overview.
  drop_target_bounds = GetDropTargetBoundsDuringDrag(window1.get());
  DragWindowTo(resizer.get(),
               drop_target_bounds.bottom_right() + gfx::Vector2d(10, 10));
  CompleteDrag(std::move(resizer));
  // |window1| should have been merged into overview.
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  // |window1|'s bounds should have been updated to its tablet mode bounds.
  EXPECT_EQ(tablet_mode_bounds, window1->bounds());
}

// Tests that window should be dropped into overview if has been dragged further
// than half of the distance from top of display to the top of drop target.
TEST_P(SplitViewTabDraggingTest, DropWindowIntoOverviewOnDragPositionTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> browser_window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  WindowState::Get(browser_window1.get())->Maximize();
  gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(browser_window1.get())
          .work_area();
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(browser_window1.get(), browser_window1.get());

  // Restore window back to maximized if it has been dragged less than the
  // distance threshold.
  gfx::Rect drop_target_bounds =
      GetDropTargetBoundsDuringDrag(browser_window1.get());
  DragWindowTo(
      resizer.get(),
      gfx::Point(
          200, work_area_bounds.y() +
                   TabletModeWindowDragDelegate::kDragPositionToOverviewRatio *
                       (drop_target_bounds.y() - work_area_bounds.y()) -
                   10));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(WindowState::Get(browser_window1.get())->IsMaximized());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  // Drop window into overview if it has beenn dragged further than the distance
  // threshold.
  resizer = StartDrag(browser_window1.get(), browser_window1.get());
  drop_target_bounds = GetDropTargetBoundsDuringDrag(browser_window1.get());
  DragWindowTo(
      resizer.get(),
      gfx::Point(
          200, work_area_bounds.y() +
                   TabletModeWindowDragDelegate::kDragPositionToOverviewRatio *
                       (drop_target_bounds.y() - work_area_bounds.y()) +
                   10));
  CompleteDrag(std::move(resizer));
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  EXPECT_TRUE(overview_session->IsWindowInOverview(browser_window1.get()));
  ToggleOverview();

  // Do not consider the drag position if preview area is shown. Window should
  // to be snapped in this case.
  resizer = StartDrag(browser_window1.get(), browser_window1.get());
  drop_target_bounds = GetDropTargetBoundsDuringDrag(browser_window1.get());
  DragWindowTo(resizer.get(), gfx::Point(0, drop_target_bounds.y() + 10));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
            GetWindowDraggingState(resizer.get()));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(WindowState::Get(browser_window1.get())->IsSnapped());
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());

  // Should not consider the drag position if splitview is active. Window should
  // still back to be snapped.
  std::unique_ptr<aura::Window> browser_window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  split_view_controller()->SnapWindow(browser_window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  resizer = StartDrag(browser_window1.get(), browser_window1.get());
  drop_target_bounds = GetDropTargetBoundsDuringDrag(browser_window1.get());
  DragWindowTo(resizer.get(), gfx::Point(0, drop_target_bounds.y() + 10));
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EndSplitView();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // Restore window back to maximized if it has been dragged less than the
  // distance threshold when dock magnifier is enabled.
  Shell::Get()->docked_magnifier_controller()->SetEnabled(true);
  work_area_bounds = display::Screen::GetScreen()
                         ->GetDisplayNearestWindow(browser_window1.get())
                         .work_area();
  resizer = StartDrag(browser_window1.get(), browser_window1.get());
  drop_target_bounds = GetDropTargetBoundsDuringDrag(browser_window1.get());
  DragWindowTo(
      resizer.get(),
      gfx::Point(
          200, work_area_bounds.y() +
                   TabletModeWindowDragDelegate::kDragPositionToOverviewRatio *
                       (drop_target_bounds.y() - work_area_bounds.y()) -
                   10));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(WindowState::Get(browser_window1.get())->IsMaximized());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that a dragged window should have the active window shadow during
// dragging.
TEST_P(SplitViewTabDraggingTest, DraggedWindowShouldHaveActiveWindowShadow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window2.get());
  window1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);

  // 1) Start dragging |window2|. |window2| is the source window.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window2.get(), window2.get());
  // |window2| should have the active window shadow.
  ::wm::ShadowController* shadow_controller = Shell::Get()->shadow_controller();
  ui::Shadow* shadow = shadow_controller->GetShadowForWindow(window2.get());
  ASSERT_TRUE(shadow);
  EXPECT_EQ(shadow->desired_elevation(), ::wm::kShadowElevationActiveWindow);
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window2.get()));

  CompleteDrag(std::move(resizer));
  Shell::Get()->shadow_controller()->UpdateShadowForWindow(window2.get());
  shadow = shadow_controller->GetShadowForWindow(window2.get());
  ASSERT_TRUE(shadow);
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window2.get()));

  // 2) Start dragging |window2|, but |window2| is not the source window.
  resizer = StartDrag(window2.get(), window1.get());
  shadow = shadow_controller->GetShadowForWindow(window2.get());
  ASSERT_TRUE(shadow);
  EXPECT_EQ(shadow->desired_elevation(), ::wm::kShadowElevationActiveWindow);
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window2.get()));

  CompleteDrag(std::move(resizer));
  Shell::Get()->shadow_controller()->UpdateShadowForWindow(window2.get());
  shadow = shadow_controller->GetShadowForWindow(window2.get());
  ASSERT_TRUE(shadow);
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window2.get()));
}

// Test that if the source window needs to be scaled up/down because of dragging
// a tab window out of it, other windows' visibilities and the home launcher's
// visibility should change accordingly.
TEST_P(SplitViewTabDraggingTest, SourceWindowBackgroundTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window4(
      CreateWindowWithType(bounds, AppType::BROWSER));
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
  EXPECT_TRUE(window4->IsVisible());

  // Home launcher should be shown because none of these windows are activated.
  EXPECT_TRUE(Shell::Get()->app_list_controller()->IsVisible(base::nullopt));

  // 1) Start dragging |window1|. |window2| is the source window.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window2.get());
  DragWindowWithOffset(resizer.get(), 10, 10);

  // Test that |window3| should be hidden now. |window1| and |window2| should
  // stay visible during dragging.
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
  EXPECT_FALSE(window4->IsVisible());

  // Test that home launcher is not shown because a window is active.
  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible(base::nullopt));

  // Test that during dragging, we could not show a hidden window.
  window3->Show();
  EXPECT_FALSE(window3->IsVisible());

  // After dragging, the windows' visibilities should have restored.
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
  EXPECT_TRUE(window4->IsVisible());

  // Test that home launcher is still not shown, because a window is active.
  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible(base::nullopt));
}

// Tests that the dragged window should be the active and top window if overview
// ended because of window drag.
TEST_P(SplitViewTabDraggingTest, OverviewEndedOnWindowDrag) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  auto* window_state1 = WindowState::Get(window1.get());
  auto* window_state2 = WindowState::Get(window2.get());

  // Drags |window2| to overview.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window2.get(), window2.get());
  gfx::Rect drop_target_bounds = GetDropTargetBoundsDuringDrag(window1.get());
  DragWindowTo(resizer.get(), drop_target_bounds.CenterPoint());
  EXPECT_TRUE(window_state2->IsSnapped());
  CompleteDrag(std::move(resizer));
  OverviewController* selector_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(selector_controller->InOverviewSession());
  EXPECT_TRUE(selector_controller->overview_session()->IsWindowInOverview(
      window2.get()));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_TRUE(window_state2->IsSnapped());

  // Drags |window1| by a small distance. Both splitview and overview should be
  // ended and |window1| is the active window and above |window2|.
  resizer = StartDrag(window1.get(), window1.get());
  DragWindowTo(resizer.get(), gfx::Point(10, 10));
  EXPECT_TRUE(window_state1->IsSnapped());
  EXPECT_TRUE(window_state2->IsSnapped());

  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(selector_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(window_state1->IsMaximized());
  EXPECT_TRUE(window_state2->IsMaximized());
  EXPECT_TRUE(window_state1->IsActive());
  EXPECT_FALSE(window_state2->IsActive());
  // |window1| should above |window2|.
  const aura::Window::Windows windows = window1->parent()->children();
  auto window1_layer = std::find(windows.begin(), windows.end(), window1.get());
  auto window2_layer = std::find(windows.begin(), windows.end(), window2.get());
  EXPECT_TRUE(window1_layer > window2_layer);
}

// When tab dragging a window, the dragged window might need to merge back into
// the source window when the drag ends. Tests the related functionalities.
TEST_P(SplitViewTabDraggingTest, MergeBackToSourceWindow) {
  UpdateDisplay("600x600");
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> dragged_window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> source_window(
      CreateWindowWithType(bounds, AppType::BROWSER));

  // 1. If splitview is not active and the dragged window is not the source
  // window.
  // a. Drag the window to less than half of the display height, and not in the
  // snap preview area.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(dragged_window.get(), source_window.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_FALSE(
      source_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(300, 200));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(
      source_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
  source_window->ClearProperty(kIsDeferredTabDraggingTargetWindowKey);

  // b. Drag the window to more than half of the display height and not in the
  // snap preview area.
  resizer = StartDrag(dragged_window.get(), source_window.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_FALSE(
      source_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(300, 500));
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(
      source_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey));

  // c. Drag the window to the snap preview area.
  resizer = StartDrag(dragged_window.get(), source_window.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_FALSE(
      source_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(0, 200));
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(
      source_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
  EndSplitView();

  // 2. If splitview is active and the dragged window is not the source window.
  // a. Drag the window to less than half of the display height, in the same
  // split of the source window, and not in the snap preview area.
  split_view_controller()->SnapWindow(source_window.get(),
                                      SplitViewController::LEFT);
  resizer = StartDrag(dragged_window.get(), source_window.get());
  EXPECT_FALSE(
      source_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(0, 200));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(
      source_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
  EndSplitView();
  source_window->ClearProperty(kIsDeferredTabDraggingTargetWindowKey);

  // b. Drag the window to less than half of the display height, in the
  // different split of the source window, and not in the snap preview area.
  split_view_controller()->SnapWindow(source_window.get(),
                                      SplitViewController::LEFT);
  resizer = StartDrag(dragged_window.get(), source_window.get());
  EXPECT_FALSE(
      source_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(500, 200));
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(
      source_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
  EndSplitView();

  // c. Drag the window to move a small distance, but is still in the different
  // split of the source window, and not in the snap preview area.
  split_view_controller()->SnapWindow(source_window.get(),
                                      SplitViewController::LEFT);
  resizer = StartDrag(dragged_window.get(), source_window.get());
  EXPECT_FALSE(
      source_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(500, 20));
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(
      source_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
  EndSplitView();
}

// Tests that if a fling event happens on a tab, the tab might or might not
// merge back into the source window depending on the fling event velocity.
TEST_P(SplitViewTabDraggingTest, FlingTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> dragged_window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> source_window(
      CreateWindowWithType(bounds, AppType::BROWSER));

  std::unique_ptr<WindowResizer> resizer =
      StartDrag(dragged_window.get(), source_window.get());
  ASSERT_TRUE(resizer.get());
  Fling(std::move(resizer), /*velocity_y=*/3000.f);
  EXPECT_FALSE(
      source_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey));

  resizer = StartDrag(dragged_window.get(), source_window.get());
  ASSERT_TRUE(resizer.get());
  Fling(std::move(resizer), /*velocity_y=*/1000.f);
  EXPECT_TRUE(
      source_window->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
}

// Tests that in various cases, after the tab drag ends, the dragged window and
// the source window should have correct bounds.
TEST_P(SplitViewTabDraggingTest, BoundsTest) {
  UpdateDisplay("600x600");
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithType(bounds, AppType::BROWSER));
  wm::ActivateWindow(window1.get());
  const gfx::Rect bounds1 = window1->bounds();
  const gfx::Rect bounds2 = window2->bounds();
  EXPECT_EQ(bounds1, bounds2);

  // 1. If splitview is not active and the dragged window is the source window.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  // Drag for a small distance.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_NE(window1->bounds(), bounds1);
  CompleteDrag(std::move(resizer));
  // The window should be maximized again and the bounds should restore to its
  // maximized window size.
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMaximized());
  EXPECT_EQ(window1->bounds(), bounds1);

  // 2. If splitview is not active and the dragged window is not the source
  // window.
  resizer = StartDrag(window1.get(), window2.get());
  // a). Drag for a small distance.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_NE(window1->bounds(), bounds1);
  EXPECT_EQ(window2->bounds(), bounds2);
  // Now drag for a longer distance so that the source window scales down.
  DragWindowTo(resizer.get(), gfx::Point(300, 200));
  EXPECT_NE(window2->bounds(), bounds2);
  CompleteDrag(std::move(resizer));
  // As in this case the dragged window should merge back to source window,
  // which we can't test here. We only test the source window's bounds restore
  // to its maximized window size.
  EXPECT_TRUE(window2->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
  EXPECT_EQ(window2->bounds(), bounds2);
  window2->ClearProperty(kIsDeferredTabDraggingTargetWindowKey);

  // b) Drag the window far enough so that the dragged window doesn't merge back
  // into the source window.
  resizer = StartDrag(window1.get(), window2.get());
  DragWindowTo(resizer.get(), gfx::Point(300, 400));
  EXPECT_NE(window1->bounds(), bounds1);
  EXPECT_NE(window2->bounds(), bounds2);
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(window2->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
  EXPECT_EQ(window1->bounds(), bounds1);
  EXPECT_EQ(window2->bounds(), bounds2);

  // 3. If splitview is active and the dragged window is the source window.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  const gfx::Rect snapped_bounds1 = window1->bounds();
  const gfx::Rect snapped_bounds2 = window2->bounds();
  resizer = StartDrag(window1.get(), window1.get());
  // Drag the window for a small distance and release.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_NE(window1->bounds(), snapped_bounds1);
  EXPECT_EQ(window2->bounds(), snapped_bounds2);
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(window1->bounds(), snapped_bounds1);
  EXPECT_EQ(window2->bounds(), snapped_bounds2);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);

  // 4. If splitview is active and the dragged window is not the source window.
  resizer = StartDrag(window3.get(), window1.get());
  // a). Drag the window for a small distance and release.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_EQ(window1->bounds(), snapped_bounds1);
  EXPECT_EQ(window2->bounds(), snapped_bounds2);
  // Drag the window for a long distance (but is still in merge-back distance
  // range), the source window should not scale down.
  DragWindowTo(resizer.get(), gfx::Point(100, 200));
  EXPECT_EQ(window1->bounds(), snapped_bounds1);
  EXPECT_EQ(window2->bounds(), snapped_bounds2);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);

  CompleteDrag(std::move(resizer));
  // In this case |window3| is supposed to merge back its source window
  // |window1|, so we only test the source window's bounds here.
  EXPECT_EQ(window1->bounds(), snapped_bounds1);
  EXPECT_EQ(window2->bounds(), snapped_bounds2);
  EXPECT_TRUE(window1->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
  window1->ClearProperty(kIsDeferredTabDraggingTargetWindowKey);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);

  // b). Drag the window far enough so that the dragged window doesn't merge
  // back into its source window.
  resizer = StartDrag(window3.get(), window1.get());
  DragWindowTo(resizer.get(), gfx::Point(100, 400));
  EXPECT_EQ(window1->bounds(), snapped_bounds1);
  EXPECT_EQ(window2->bounds(), snapped_bounds2);
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(window1->GetProperty(kIsDeferredTabDraggingTargetWindowKey));
  // |window3| replaced |window1| as the left snapped window.
  EXPECT_EQ(window3->bounds(), snapped_bounds1);
  EXPECT_EQ(window2->bounds(), snapped_bounds2);
}

// Tests that press overview key in keyboard during drag should not put the
// dragged window into overview.
TEST_P(SplitViewTabDraggingTest, PressOverviewKeyDuringDrag) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> dragged_window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  WindowState::Get(dragged_window.get())->Maximize();
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(dragged_window.get(), dragged_window.get());
  DragWindowTo(resizer.get(), gfx::Point(300, 300));
  EXPECT_TRUE(WindowState::Get(dragged_window.get())->is_dragged());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  GetEventGenerator()->PressKey(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_FALSE(Shell::Get()
                   ->overview_controller()
                   ->overview_session()
                   ->IsWindowInOverview(dragged_window.get()));
  EXPECT_TRUE(WindowState::Get(dragged_window.get())->is_dragged());
  resizer->CompleteDrag();
}

// Tests that if the dragged window is activated after the drag ends, but before
// the dragged window gets snapped, the divider bar is placed correctly above
// the snapped windows.
TEST_P(SplitViewTabDraggingTest, DragActiveWindow) {
  UpdateDisplay("600x600");
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithType(bounds, AppType::BROWSER));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ToggleOverview();
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  std::unique_ptr<aura::Window> dragged_window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  std::unique_ptr<WindowResizer> resizer =
      StartDrag(dragged_window.get(), window1.get());

  // Drag window to the other side of the split screen.
  DragWindowTo(resizer.get(), gfx::Point(580, 600));
  resizer->CompleteDrag();

  // To simulate what might happen in real situation, we activate the dragged
  // window first before clearing the window's tab dragging properties.
  wm::ActivateWindow(dragged_window.get());
  SetIsInTabDragging(resizer->GetTarget(), /*is_dragging=*/false);

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
}

// Tests that the divider bar should be placed on top after the drag ends, no
// matter the dragged window is destroyed during the drag or not.
TEST_P(SplitViewTabDraggingTest, DividerBarOnTopAfterDragEnds) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> dragged_window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> another_window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  split_view_controller()->SnapWindow(dragged_window.get(),
                                      SplitViewController::LEFT);
  split_view_controller()->SnapWindow(another_window.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);

  // If the dragged window stays as a separate window after drag ends:
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(dragged_window.get(), dragged_window.get());
  DragWindowWithOffset(resizer.get(), 10, 10);
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());

  // If the dragged window is destroyed after drag ends:
  resizer = StartDrag(dragged_window.get(), dragged_window.get());
  DragWindowWithOffset(resizer.get(), 10, 10);
  resizer->CompleteDrag();
  resizer.reset();
  dragged_window.reset();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kRightSnapped);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_NE(ui::ZOrderLevel::kNormal,
            split_view_divider()->divider_widget()->GetZOrderLevel());
}

TEST_P(SplitViewTabDraggingTest, IgnoreActivatedTabDraggingWindow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> dragged_window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> left_window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> right_window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  split_view_controller()->SnapWindow(left_window.get(),
                                      SplitViewController::LEFT);
  split_view_controller()->SnapWindow(right_window.get(),
                                      SplitViewController::RIGHT);

  // Simulate what might happen in reality.
  SetIsInTabDragging(dragged_window.get(), /*is_dragging=*/true,
                     left_window.get());
  wm::ActivateWindow(dragged_window.get());

  // Test that |left_window| and |right_window| is still snapped in splitview
  // and overview is not opened behind the dragged window.
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(left_window.get()));
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(right_window.get()));
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

class SplitViewTabDraggingTestWithClamshellSupport
    : public SplitViewTabDraggingTest {
 public:
  SplitViewTabDraggingTestWithClamshellSupport() = default;
  ~SplitViewTabDraggingTestWithClamshellSupport() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDragToSnapInClamshellMode);
    SplitViewTabDraggingTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewTabDraggingTestWithClamshellSupport);
};

TEST_P(SplitViewTabDraggingTestWithClamshellSupport,
       DragTabFromSnappedWindowToOverviewAndThenExitTablet) {
  // Snap a browser window in split view.
  std::unique_ptr<aura::Window> snapped_window(
      CreateWindowWithType(gfx::Rect(), AppType::BROWSER));
  ToggleOverview();
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SplitViewController::LEFT);

  // Drag a tab out of the browser window and into overview.
  std::unique_ptr<aura::Window> dragged_tab(
      CreateWindowWithType(gfx::Rect(), AppType::BROWSER));
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(dragged_tab.get(), snapped_window.get());
  DragWindowTo(resizer.get(),
               gfx::ToEnclosingRect(
                   Shell::Get()
                       ->overview_controller()
                       ->overview_session()
                       ->GetGridWithRootWindow(snapped_window->GetRootWindow())
                       ->GetDropTarget()
                       ->target_bounds())
                   .CenterPoint());
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(dragged_tab->GetProperty(kIsShowingInOverviewKey));

  // Switch to clamshell mode and check that |snapped_window| keeps its snapped
  // window state.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_EQ(WindowStateType::kLeftSnapped,
            WindowState::Get(snapped_window.get())->GetStateType());
}

class TestWindowDelegateWithWidget : public views::WidgetDelegate {
 public:
  TestWindowDelegateWithWidget(bool can_resize) : can_resize_(can_resize) {
    SetFocusTraversesOut(true);
  }
  ~TestWindowDelegateWithWidget() override = default;

  // views::WidgetDelegate:
  void DeleteDelegate() override { delete this; }
  views::Widget* GetWidget() override { return widget_; }
  const views::Widget* GetWidget() const override { return widget_; }
  bool CanActivate() const override { return true; }
  bool CanResize() const override { return can_resize_; }
  bool CanMaximize() const override { return true; }

  void set_widget(views::Widget* widget) { widget_ = widget; }

 private:
  bool can_resize_;
  views::Widget* widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegateWithWidget);
};

class SplitViewAppDraggingTest : public SplitViewControllerTest {
 public:
  SplitViewAppDraggingTest() = default;
  ~SplitViewAppDraggingTest() override = default;

  // SplitViewControllerTest:
  void TearDown() override {
    window_.reset();
    controller_.reset();
    SplitViewControllerTest::TearDown();
  }

 protected:
  std::unique_ptr<aura::Window> CreateTestWindowWithWidget(bool can_resize) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.show_state = ui::SHOW_STATE_MAXIMIZED;
    views::Widget* widget = new views::Widget;
    std::unique_ptr<TestWindowDelegateWithWidget> widget_delegate =
        std::make_unique<TestWindowDelegateWithWidget>(can_resize);
    widget_delegate->set_widget(widget);
    params.delegate = widget_delegate.release();
    params.context = GetContext();
    widget->Init(std::move(params));
    widget->Show();
    return base::WrapUnique<aura::Window>(widget->GetNativeView());
  }

  void InitializeWindow(bool can_resize = true) {
    window_ = CreateTestWindowWithWidget(can_resize);
  }

  // Sends a gesture scroll sequence to TabletModeAppWindowDragController.
  void SendGestureEvents(const gfx::PointF& location) {
    SendScrollStartAndUpdate(location);
    EndScrollSequence();
  }

  void SendScrollStartAndUpdate(const gfx::PointF& location) {
    WindowState* window_state = WindowState::Get(window());
    window_state->CreateDragDetails(location, HTCAPTION,
                                    ::wm::WINDOW_MOVE_SOURCE_TOUCH);
    controller_ = std::make_unique<TabletModeWindowResizer>(
        window_state, std::make_unique<TabletModeBrowserWindowDragDelegate>());
    controller_->drag_delegate_for_testing()
        ->set_drag_start_deadline_for_testing(base::Time::Now());
    controller_->Drag(location, 0);
  }

  void EndScrollSequence() {
    controller_->CompleteDrag();
    WindowState::Get(window())->DeleteDragDetails();
  }

  void Fling(const gfx::PointF& location, float velocity_y, float velocity_x) {
    ui::GestureEvent event = ui::GestureEvent(
        location.x(), location.y(), ui::EF_NONE, base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::ET_SCROLL_FLING_START, velocity_x,
                                velocity_y));
    ui::Event::DispatcherApi(&event).set_target(window());
    controller_->FlingOrSwipe(&event);
    WindowState::Get(window())->DeleteDragDetails();
  }

  SplitViewDragIndicators::WindowDraggingState GetWindowDraggingState() {
    return controller_->drag_delegate_for_testing()
        ->split_view_drag_indicators_for_testing()
        ->current_window_dragging_state();
  }

  aura::Window* window() { return window_.get(); }

  std::unique_ptr<TabletModeWindowResizer> controller_;

  std::unique_ptr<aura::Window> window_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SplitViewAppDraggingTest);
};

// Tests that drag the window that cannot be snapped from top of the display
// will not snap the window into splitscreen.
TEST_P(SplitViewAppDraggingTest, DragNonActiveMaximizedWindow) {
  UpdateDisplay("800x600");
  InitializeWindow(false);
  EXPECT_TRUE(WindowState::Get(window())->IsMaximized());
  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window());
  const float long_scroll_delta = display_bounds.height() / 4 + 5;

  const gfx::PointF location(0, long_scroll_delta);
  // Drag the window that cannot be snapped long enough, the window will be
  // dropped into overview.
  SendScrollStartAndUpdate(location);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(
      overview_controller->overview_session()->IsWindowInOverview(window()));
  EndScrollSequence();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(
      overview_controller->overview_session()->IsWindowInOverview(window()));
}

// Tests the functionalities that are related to dragging a maximized window
// into splitscreen.
TEST_P(SplitViewAppDraggingTest, DragActiveMaximizedWindow) {
  UpdateDisplay("800x600");
  InitializeWindow();
  EXPECT_TRUE(WindowState::Get(window())->IsMaximized());
  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window());

  // Move the window by a small amount of distance will maximize the window
  // again.
  gfx::PointF location(0, 10);
  SendGestureEvents(location);
  EXPECT_TRUE(WindowState::Get(window())->IsMaximized());

  // Drag the window long enough (pass one fourth of the screen vertical
  // height) to snap the window to splitscreen.
  const float long_scroll_delta = display_bounds.height() / 4 + 5;
  location.set_y(long_scroll_delta);
  SendScrollStartAndUpdate(location);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(
      overview_controller->overview_session()->IsWindowInOverview(window()));
  EndScrollSequence();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->left_window(), window());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_TRUE(WindowState::Get(window())->IsSnapped());

  // FLING the window with small velocity (smaller than
  // kFlingToOverviewThreshold) will not able to drop the window into overview.
  location.set_y(10);
  SendScrollStartAndUpdate(location);
  overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  Fling(location,
        TabletModeWindowDragDelegate::kFlingToOverviewThreshold - 10.f, 0);
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // FLING the window with large velocity (larger than
  // kFlingToOverviewThreshold) will drop the window into overview.
  SendScrollStartAndUpdate(location);
  overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  Fling(location,
        TabletModeWindowDragDelegate::kFlingToOverviewThreshold + 10.f, 0);
  EXPECT_TRUE(overview_controller->InOverviewSession());
}

// Tests the shelf visibility when a fullscreened window is being dragged.
TEST_P(SplitViewAppDraggingTest, ShelfVisibilityIfDraggingFullscreenedWindow) {
  UpdateDisplay("800x600");
  InitializeWindow();
  ShelfLayoutManager* shelf_layout_manager =
      AshTestBase::GetPrimaryShelf()->shelf_layout_manager();
  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window());

  // Shelf will be auto-hidden if the window requests to be fullscreened.
  WindowState* window_state = WindowState::Get(window());
  const WMEvent fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&fullscreen_event);
  window_state->SetHideShelfWhenFullscreen(false);
  window()->SetProperty(kImmersiveIsActive, true);
  shelf_layout_manager->UpdateVisibilityState();
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_FALSE(shelf_layout_manager->IsVisible());

  // Drag the window by a small amount of distance, the window will back to
  // fullscreened, and shelf will be hidden again.
  gfx::PointF location(0, 10);
  SendGestureEvents(location);
  shelf_layout_manager->UpdateVisibilityState();
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_FALSE(shelf_layout_manager->IsVisible());

  // Shelf is visible during dragging.
  location.set_y(display_bounds.height() / 4 + 5);
  SendScrollStartAndUpdate(location);
  EXPECT_TRUE(shelf_layout_manager->IsVisible());
  EndScrollSequence();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(WindowState::Get(window())->IsSnapped());
  EXPECT_TRUE(shelf_layout_manager->IsVisible());
}

// Tests the auto-hide shelf state during window dragging.
TEST_P(SplitViewAppDraggingTest, AutoHideShelf) {
  UpdateDisplay("800x600");
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  InitializeWindow();
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  ShelfLayoutManager* shelf_layout_manager =
      AshTestBase::GetPrimaryShelf()->shelf_layout_manager();
  shelf_layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_TRUE(WindowState::Get(window())->IsMaximized());

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  shelf_layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  const gfx::PointF location(0, display_bounds.height() / 4 + 5);
  SendScrollStartAndUpdate(location);
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  // Shelf should be shown during drag.
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EndScrollSequence();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  // Shelf should be shown after drag and snapped window should be covered by
  // the auto-hide-shown shelf.
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(
      split_view_controller()
          ->GetSnappedWindowBoundsInScreen(SplitViewController::LEFT, window())
          .height(),
      display_bounds.height());
}

// Tests the functionalities that fling the window when preview area is shown.
TEST_P(SplitViewAppDraggingTest, FlingWhenPreviewAreaIsShown) {
  InitializeWindow();
  EXPECT_TRUE(WindowState::Get(window())->IsMaximized());
  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window());

  const float long_scroll_delta = display_bounds.height() / 4 + 5;
  float large_velocity =
      TabletModeWindowDragDelegate::kFlingToOverviewFromSnappingAreaThreshold +
      10.f;
  float small_velocity =
      TabletModeWindowDragDelegate::kFlingToOverviewFromSnappingAreaThreshold -
      10.f;

  // Fling to the right with large enough velocity when trying to snap the
  // window to the left should drop the window to overview.
  gfx::PointF location(0, long_scroll_delta);
  SendScrollStartAndUpdate(location);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
            GetWindowDraggingState());
  Fling(location, /*velocity_y*/ 0, /*velocity_x=*/large_velocity);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  OverviewSession* overview_session = overview_controller->overview_session();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_session->IsWindowInOverview(window()));
  ToggleOverview();
  EXPECT_TRUE(WindowState::Get(window())->IsMaximized());

  // Fling to the right with small velocity when trying to snap the
  // window to the left should still snap the window to left.
  SendScrollStartAndUpdate(location);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
            GetWindowDraggingState());
  Fling(location, /*velocity_y*/ 0, /*velocity_x=*/small_velocity);
  EXPECT_TRUE(WindowState::Get(window())->IsSnapped());
  EndSplitView();
  EXPECT_TRUE(WindowState::Get(window())->IsMaximized());

  // Fling to the left with large enough velocity when trying to snap the window
  // to the right should drop the window to overvie.
  location = gfx::PointF(display_bounds.right(), long_scroll_delta);
  SendScrollStartAndUpdate(location);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapRight,
            GetWindowDraggingState());
  Fling(location, /*velocity_y*/ 0, /*velocity_x=*/-large_velocity);
  overview_session = overview_controller->overview_session();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_session->IsWindowInOverview(window()));
  ToggleOverview();

  // Fling to the left with small velocity when trying to snap the
  // window to the right should still snap the window to right.
  SendScrollStartAndUpdate(location);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapRight,
            GetWindowDraggingState());
  Fling(location, /*velocity_y*/ 0, /*velocity_x=*/-small_velocity);
  EXPECT_TRUE(WindowState::Get(window())->IsSnapped());
}

// Tests the functionalities that fling a window when splitview is active.
TEST_P(SplitViewAppDraggingTest, FlingWhenSplitViewIsActive) {
  InitializeWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindowWithWidget(true);

  split_view_controller()->SnapWindow(window(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window());
  const float long_scroll_y = display_bounds.bottom() - 10;
  float large_velocity =
      TabletModeWindowDragDelegate::kFlingToOverviewFromSnappingAreaThreshold +
      10.f;

  // Fling the window in left snapping area to left should still snap the
  // window.
  gfx::PointF location(0, long_scroll_y);
  SendScrollStartAndUpdate(location);
  Fling(location, /*velocity_y=*/0, /*velocity_x=*/-large_velocity);
  EXPECT_TRUE(WindowState::Get(window())->IsSnapped());
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());

  // Fling the window in left snapping area to right should drop the window
  // into overview.
  SendScrollStartAndUpdate(location);
  Fling(location, /*velocity_y=*/0, /*velocity_x=*/large_velocity);
  OverviewController* selector_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(
      selector_controller->overview_session()->IsWindowInOverview(window()));
  EXPECT_EQ(SplitViewController::State::kRightSnapped,
            split_view_controller()->state());
  EXPECT_TRUE(IsTabletMode());
  ToggleOverview();
  EXPECT_TRUE(IsTabletMode());

  // If the window is to the left of the divider but outside the left snapping
  // area, then flinging to the left should drop the window into overview (like
  // just ending the drag).
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  location.set_x(display_bounds.CenterPoint().x() - 10);
  SendScrollStartAndUpdate(location);
  Fling(location, /*velocity_y=*/0, /*velocity_x=*/-large_velocity);
  EXPECT_TRUE(
      selector_controller->overview_session()->IsWindowInOverview(window()));
  EXPECT_EQ(SplitViewController::State::kRightSnapped,
            split_view_controller()->state());
  ToggleOverview();

  // If the window is to the left of the divider but outside the left snapping
  // area, then flinging to the right should drop the window into overview (like
  // just ending the drag).
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  SendScrollStartAndUpdate(location);
  Fling(location, /*velocity_y=*/0, /*velocity_x=*/large_velocity);
  EXPECT_TRUE(
      selector_controller->overview_session()->IsWindowInOverview(window()));
  EXPECT_EQ(SplitViewController::State::kRightSnapped,
            split_view_controller()->state());
  ToggleOverview();

  // If the window is to the right of the divider but outside the right snapping
  // area, then flinging to the left should drop the window into overview (like
  // just ending the drag).
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  location.set_x(display_bounds.CenterPoint().x() + 10);
  SendScrollStartAndUpdate(location);
  Fling(location, /*velocity_y=*/0, /*velocity_x=*/-large_velocity);
  EXPECT_TRUE(
      selector_controller->overview_session()->IsWindowInOverview(window()));
  EXPECT_EQ(SplitViewController::State::kRightSnapped,
            split_view_controller()->state());
  ToggleOverview();

  // If the window is to the right of the divider but outside the right snapping
  // area, then flinging to the right should drop the window into overview (like
  // just ending the drag).
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  SendScrollStartAndUpdate(location);
  Fling(location, /*velocity_y=*/0, /*velocity_x=*/large_velocity);
  EXPECT_TRUE(
      selector_controller->overview_session()->IsWindowInOverview(window()));
  EXPECT_EQ(SplitViewController::State::kRightSnapped,
            split_view_controller()->state());
  ToggleOverview();

  // Fling the window in right snapping area to left should drop the window into
  // overview.
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  location.set_x(display_bounds.right() - 1);
  SendScrollStartAndUpdate(location);
  Fling(location, /*velocity_y=*/0, /*velocity_x=*/-large_velocity);
  EXPECT_TRUE(
      selector_controller->overview_session()->IsWindowInOverview(window()));
  EXPECT_EQ(SplitViewController::State::kRightSnapped,
            split_view_controller()->state());
  ToggleOverview();

  // Fling the window in right snapping area to right should snap the window to
  // right side.
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  SendScrollStartAndUpdate(location);
  Fling(location, /*velocity_y=*/0, /*velocity_x=*/large_velocity);
  EXPECT_EQ(split_view_controller()->right_window(), window());
  EXPECT_TRUE(selector_controller->overview_session()->IsWindowInOverview(
      window2.get()));
  EXPECT_EQ(SplitViewController::State::kRightSnapped,
            split_view_controller()->state());
}

// Tests the backdrop bounds during window drag.
TEST_P(SplitViewAppDraggingTest, BackdropBoundsDuringDrag) {
  InitializeWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindowWithWidget(true);
  split_view_controller()->SnapWindow(window(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(window2.get(), window_util::GetActiveWindow());

  const aura::Window* active_desk_container =
      Shell::GetPrimaryRootWindowController()->GetContainer(
          desks_util::GetActiveDeskContainerId());

  // Backdrop window should below two snapped windows and its bounds should be
  // the same as the container bounds.
  EXPECT_EQ(3U, active_desk_container->children().size());
  EXPECT_EQ(window(), active_desk_container->children()[1]);
  EXPECT_EQ(window2.get(), active_desk_container->children()[2]);
  EXPECT_EQ(active_desk_container->bounds(),
            active_desk_container->children()[0]->bounds());

  // Start window drag and activate the dragged window during drag.
  gfx::PointF location(0, 10);
  SendScrollStartAndUpdate(location);
  wm::ActivateWindow(window());

  aura::Window::Windows windows = active_desk_container->children();
  auto it = std::find(windows.begin(), windows.end(), window2.get());
  // Backdrop window should be the window that just below the snapped |window2|
  // and its bounds should be the same as the snapped window during drag.
  aura::Window* backdrop_window = nullptr;
  if (it != windows.begin())
    backdrop_window = *(--it);
  DCHECK(backdrop_window);
  EXPECT_EQ(window2->bounds(), backdrop_window->bounds());

  // Backdrop should restore back to container bounds after drag.
  EndScrollSequence();
  EXPECT_EQ(window(), window_util::GetActiveWindow());
  windows = active_desk_container->children();
  it = std::find(windows.begin(), windows.end(), window2.get());
  if (it != windows.begin())
    backdrop_window = *(--it);
  DCHECK(backdrop_window);
  EXPECT_EQ(backdrop_window->bounds(), active_desk_container->bounds());
}

INSTANTIATE_TEST_SUITE_P(All, SplitViewControllerTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, SplitViewTabDraggingTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         SplitViewTabDraggingTestWithClamshellSupport,
                         testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, SplitViewAppDraggingTest, testing::Bool());

}  // namespace ash
