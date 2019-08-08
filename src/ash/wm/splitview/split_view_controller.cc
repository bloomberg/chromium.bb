// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_controller.h"

#include <cmath>
#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_data.h"
#include "ash/system/toast/toast_manager.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Three fixed position ratios of the divider, which means the divider can
// always be moved to these three positions.
constexpr float kFixedPositionRatios[] = {0.f, 0.5f, 1.0f};

// Two optional position ratios of the divider. Whether the divider can be moved
// to these two positions depends on the minimum size of the snapped windows.
constexpr float kOneThirdPositionRatio = 0.33f;
constexpr float kTwoThirdPositionRatio = 0.67f;

// The black scrim starts to fade in when the divider is moved past the two
// optional positions (kOneThirdPositionRatio, kTwoThirdPositionRatio) and
// reaches to its maximum opacity (kBlackScrimOpacity) after moving
// kBlackScrimFadeInRatio of the screen width. See https://crbug.com/827730 for
// details.
constexpr float kBlackScrimFadeInRatio = 0.1f;
constexpr float kBlackScrimOpacity = 0.4f;

// The duration of the divider snap animation, in milliseconds. Before you
// change this value, read the comment on kIsWindowMovedTimeoutMs in
// tablet_mode_window_drag_delegate.cc.
constexpr int kDividerSnapDurationMs = 300;

// Toast data.
constexpr char kAppCannotSnapToastId[] = "split_view_app_cannot_snap";
constexpr int kAppCannotSnapToastDurationMs = 2500;

// Histogram names that record presentation time of resize operation with
// following conditions, a) single snapped window, empty overview, b) two
// snapped windows, c) single snapped window and non empty overview.
constexpr char kSplitViewResizeSingleHistogram[] =
    "Ash.SplitViewResize.PresentationTime.TabletMode.SingleWindow";
constexpr char kSplitViewResizeMultiHistogram[] =
    "Ash.SplitViewResize.PresentationTime.TabletMode.MultiWindow";
constexpr char kSplitViewResizeWithOverviewHistogram[] =
    "Ash.SplitViewResize.PresentationTime.TabletMode.WithOverview";

constexpr char kSplitViewResizeSingleMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.SingleWindow";
constexpr char kSplitViewResizeMultiMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.MultiWindow";
constexpr char kSplitViewResizeWithOverviewMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.TabletMode.WithOverview";

gfx::Point GetBoundedPosition(const gfx::Point& location_in_screen,
                              const gfx::Rect& bounds_in_screen) {
  return gfx::Point(
      std::max(std::min(location_in_screen.x(), bounds_in_screen.right() - 1),
               bounds_in_screen.x()),
      std::max(std::min(location_in_screen.y(), bounds_in_screen.bottom() - 1),
               bounds_in_screen.y()));
}

mojom::SplitViewState ToMojomSplitViewState(SplitViewController::State state) {
  switch (state) {
    case SplitViewController::NO_SNAP:
      return mojom::SplitViewState::NO_SNAP;
    case SplitViewController::LEFT_SNAPPED:
      return mojom::SplitViewState::LEFT_SNAPPED;
    case SplitViewController::RIGHT_SNAPPED:
      return mojom::SplitViewState::RIGHT_SNAPPED;
    case SplitViewController::BOTH_SNAPPED:
      return mojom::SplitViewState::BOTH_SNAPPED;
    default:
      NOTREACHED();
      return mojom::SplitViewState::NO_SNAP;
  }
}

mojom::WindowStateType GetStateTypeFromSnapPosition(
    SplitViewController::SnapPosition snap_position) {
  if (snap_position == SplitViewController::LEFT)
    return mojom::WindowStateType::LEFT_SNAPPED;
  if (snap_position == SplitViewController::RIGHT)
    return mojom::WindowStateType::RIGHT_SNAPPED;
  return mojom::WindowStateType::DEFAULT;
}

// Returns the minimum size of the window according to the screen orientation.
int GetMinimumWindowSize(aura::Window* window, bool is_landscape) {
  int minimum_width = 0;
  if (window && window->delegate()) {
    gfx::Size minimum_size = window->delegate()->GetMinimumSize();
    minimum_width = is_landscape ? minimum_size.width() : minimum_size.height();
  }
  return minimum_width;
}

// Returns true if |window| is currently snapped.
bool IsSnapped(aura::Window* window) {
  if (!window)
    return false;
  return wm::GetWindowState(window)->IsSnapped();
}

// Returns the overview session if overview mode is active, otherwise returns
// nullptr.
OverviewSession* GetOverviewSession() {
  return Shell::Get()->overview_controller()->IsSelecting()
             ? Shell::Get()->overview_controller()->overview_session()
             : nullptr;
}

}  // namespace

// The window observer that observes the current tab-dragged window. When it's
// created, it observes the dragged window, and there are two possible results
// after the user finishes dragging: 1) the dragged window stays a new window
// and SplitViewController needs to decide where to put the window; 2) the
// dragged window's tabs are attached into another browser window and thus is
// destroyed.
class SplitViewController::TabDraggedWindowObserver
    : public aura::WindowObserver {
 public:
  TabDraggedWindowObserver(
      SplitViewController* split_view_controller,
      aura::Window* dragged_window,
      SplitViewController::SnapPosition desired_snap_position,
      const gfx::Point& last_location_in_screen)
      : split_view_controller_(split_view_controller),
        dragged_window_(dragged_window),
        desired_snap_position_(desired_snap_position),
        last_location_in_screen_(last_location_in_screen) {
    DCHECK(wm::IsDraggingTabs(dragged_window_));
    dragged_window_->AddObserver(this);
  }

  ~TabDraggedWindowObserver() override {
    if (dragged_window_)
      dragged_window_->RemoveObserver(this);
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    // At this point we know the newly created dragged window is going to be
    // destroyed due to all of its tabs are attaching into another window.
    EndTabDragging(window, /*is_being_destroyed=*/true);
  }

  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK_EQ(window, dragged_window_);
    if (key == ash::kIsDraggingTabsKey && !wm::IsDraggingTabs(window)) {
      // At this point we know the newly created dragged window just finished
      // dragging.
      EndTabDragging(window, /*is_being_destroyed=*/false);
    }
  }

 private:
  // Called after the tab dragging is ended, the dragged window is either
  // destroyed because of merging into another window, or stays as a separate
  // window.
  void EndTabDragging(aura::Window* window, bool is_being_destroyed) {
    dragged_window_->RemoveObserver(this);
    dragged_window_ = nullptr;
    split_view_controller_->EndWindowDragImpl(window, is_being_destroyed,
                                              desired_snap_position_,
                                              last_location_in_screen_);

    // Update the source window's bounds if applicable.
    UpdateSourceWindowBoundsAfterDragEnds(window);
  }

  // The source window might have been scaled down during dragging, we should
  // update its bounds to ensure it has the right bounds after the drag ends.
  void UpdateSourceWindowBoundsAfterDragEnds(aura::Window* window) {
    aura::Window* source_window =
        window->GetProperty(ash::kTabDraggingSourceWindowKey);
    if (source_window) {
      TabletModeWindowState::UpdateWindowPosition(
          wm::GetWindowState(source_window), /*animate=*/true);
    }
  }

  SplitViewController* split_view_controller_;
  aura::Window* dragged_window_;
  SplitViewController::SnapPosition desired_snap_position_;
  gfx::Point last_location_in_screen_;

  DISALLOW_COPY_AND_ASSIGN(TabDraggedWindowObserver);
};

// Animates the divider to its closest fixed position.
// SplitViewController::is_resizing_ is assumed to be already set to false
// before this animation starts, but some resizing logic is delayed until this
// animation ends.
class SplitViewController::DividerSnapAnimation
    : public gfx::SlideAnimation,
      public gfx::AnimationDelegate {
 public:
  DividerSnapAnimation(SplitViewController* split_view_controller,
                       int starting_position,
                       int ending_position)
      : gfx::SlideAnimation(this),
        split_view_controller_(split_view_controller),
        starting_position_(starting_position),
        ending_position_(ending_position) {
    SetSlideDuration(kDividerSnapDurationMs);
    SetTweenType(gfx::Tween::EASE_IN);
  }

  ~DividerSnapAnimation() override = default;

  int ending_position() const { return ending_position_; }

 private:
  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override {
    DCHECK(split_view_controller_->IsSplitViewModeActive());
    DCHECK(!split_view_controller_->is_resizing_);
    DCHECK_EQ(ending_position_, split_view_controller_->divider_position_);

    split_view_controller_->EndResizeImpl();
    split_view_controller_->EndSplitViewAfterResizingIfAppropriate();
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    DCHECK(split_view_controller_->IsSplitViewModeActive());
    DCHECK(!split_view_controller_->is_resizing_);

    split_view_controller_->divider_position_ =
        CurrentValueBetween(starting_position_, ending_position_);
    split_view_controller_->NotifyDividerPositionChanged();
    split_view_controller_->UpdateSnappedWindowsAndDividerBounds();
    split_view_controller_->SetWindowsTransformDuringResizing();
  }

  SplitViewController* split_view_controller_;
  int starting_position_;
  int ending_position_;
};

SplitViewController::SplitViewController() {
  Shell::Get()->accessibility_controller()->AddObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  if (IsClamshellSplitViewModeEnabled()) {
    split_view_type_ = Shell::Get()
                               ->tablet_mode_controller()
                               ->IsTabletModeWindowManagerEnabled()
                           ? SplitViewType::kTabletType
                           : SplitViewType::kClamshellType;
  }
}

SplitViewController::~SplitViewController() {
  display::Screen::GetScreen()->RemoveObserver(this);
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
  EndSplitView();
}

void SplitViewController::BindRequest(
    mojom::SplitViewControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

bool SplitViewController::IsSplitViewModeActive() const {
  return InClamshellSplitViewMode() || InTabletSplitViewMode();
}

bool SplitViewController::InClamshellSplitViewMode() const {
  return state_ != NO_SNAP && split_view_type_ == SplitViewType::kClamshellType;
}

bool SplitViewController::InTabletSplitViewMode() const {
  return state_ != NO_SNAP && split_view_type_ == SplitViewType::kTabletType;
}

void SplitViewController::SnapWindow(aura::Window* window,
                                     SnapPosition snap_position) {
  DCHECK(window && CanSnapInSplitview(window));
  DCHECK_NE(snap_position, NONE);
  DCHECK(!is_resizing_);

  // This check detects the case that you try to snap a window while watching
  // the divider snap animation. It also detects the case that you click a
  // window in overview while tap dragging the divider (possible by using the
  // emulator or chrome://flags/#force-tablet-mode).
  if (IsDividerAnimating())
    return;

  UpdateSnappingWindowTransformedBounds(window);
  RemoveWindowFromOverviewIfApplicable(window);

  if (state_ == NO_SNAP) {
    // Add observers when the split view mode starts.
    Shell::Get()->AddShellObserver(this);
    Shell::Get()->overview_controller()->AddObserver(this);
    Shell::Get()->activation_client()->AddObserver(this);
    Shell::Get()->NotifySplitViewModeStarting();

    divider_position_ = GetDefaultDividerPosition(window);
    default_snap_position_ = snap_position;

    // There is no divider bar in clamshell splitview mode.
    if (split_view_type_ == SplitViewType::kTabletType) {
      split_view_divider_ =
          std::make_unique<SplitViewDivider>(this, window->GetRootWindow());
    }
    splitview_start_time_ = base::Time::Now();
  }

  aura::Window* previous_snapped_window = nullptr;
  if (snap_position == LEFT) {
    if (left_window_ != window) {
      previous_snapped_window = left_window_;
      StopObserving(LEFT);
      left_window_ = window;
    }
    if (right_window_ == window) {
      right_window_ = nullptr;
      default_snap_position_ = LEFT;
    }
  } else if (snap_position == RIGHT) {
    if (right_window_ != window) {
      previous_snapped_window = right_window_;
      StopObserving(RIGHT);
      right_window_ = window;
    }
    if (left_window_ == window) {
      left_window_ = nullptr;
      default_snap_position_ = RIGHT;
    }
  }
  StartObserving(window);

  if (split_view_type_ == SplitViewType::kTabletType) {
    // Insert the previous snapped window to overview if overview is active.
    if (previous_snapped_window)
      InsertWindowToOverview(previous_snapped_window);

    // Update the divider position and window bounds before snapping a new
    // window. Since the minimum size of |window| maybe larger than currently
    // bounds in |snap_position|.
    if (state_ != NO_SNAP) {
      divider_position_ = GetClosestFixedDividerPosition();
      UpdateSnappedWindowsAndDividerBounds();
    }
  }

  if (wm::GetWindowState(window)->GetStateType() ==
      GetStateTypeFromSnapPosition(snap_position)) {
    // Update its snapped bounds as its bounds may not be the expected snapped
    // bounds here.
    const wm::WMEvent event((snap_position == LEFT) ? wm::WM_EVENT_SNAP_LEFT
                                                    : wm::WM_EVENT_SNAP_RIGHT);
    wm::GetWindowState(window)->OnWMEvent(&event);

    OnWindowSnapped(window);
  } else {
    // Otherwise, try to snap it first. It will be activated later after the
    // window is snapped. The split view state will also be updated after the
    // window is snapped.
    const wm::WMEvent event((snap_position == LEFT) ? wm::WM_EVENT_SNAP_LEFT
                                                    : wm::WM_EVENT_SNAP_RIGHT);
    wm::GetWindowState(window)->OnWMEvent(&event);
  }

  base::RecordAction(base::UserMetricsAction("SplitView_SnapWindow"));
}

void SplitViewController::SwapWindows() {
  DCHECK(IsSplitViewModeActive());

  // Ignore |is_resizing_| because it will be true in case of double tapping
  // (not double clicking) the divider without ever actually dragging it
  // anywhere. Double tapping the divider triggers StartResize(), EndResize(),
  // StartResize(), SwapWindows(), EndResize(). Double clicking the divider
  // (possible by using the emulator or chrome://flags/#force-tablet-mode)
  // triggers StartResize(), EndResize(), StartResize(), EndResize(),
  // SwapWindows(). Those two sequences of function calls are what were mainly
  // considered in writing the condition for bailing out here, to disallow
  // swapping windows when the divider is being dragged or is animating.
  if (IsDividerAnimating())
    return;

  aura::Window* new_left_window = right_window_;
  aura::Window* new_right_window = left_window_;
  left_window_ = new_left_window;
  right_window_ = new_right_window;

  // Update |default_snap_position_| if necessary.
  if (!left_window_ || !right_window_)
    default_snap_position_ = left_window_ ? LEFT : RIGHT;

  divider_position_ = GetClosestFixedDividerPosition();
  UpdateSnappedWindowsAndDividerBounds();
  UpdateSplitViewStateAndNotifyObservers();

  base::RecordAction(
      base::UserMetricsAction("SplitView_DoubleTapDividerSwapWindows"));
}

aura::Window* SplitViewController::GetDefaultSnappedWindow() {
  if (default_snap_position_ == LEFT)
    return left_window_;
  if (default_snap_position_ == RIGHT)
    return right_window_;
  return nullptr;
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInParent(
    aura::Window* window,
    SnapPosition snap_position) {
  gfx::Rect bounds = GetSnappedWindowBoundsInScreen(window, snap_position);
  ::wm::ConvertRectFromScreen(window->GetRootWindow(), &bounds);
  return bounds;
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInScreen(
    aura::Window* window,
    SnapPosition snap_position) {
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window);
  if (snap_position == NONE)
    return work_area_bounds_in_screen;

  gfx::Rect left_or_top_rect, right_or_bottom_rect;
  GetSnappedWindowBoundsInScreenInternal(window, &left_or_top_rect,
                                         &right_or_bottom_rect);

  // Adjust the bounds for |left_or_top_rect| and |right_or_bottom_rect| if the
  // desired bound is smaller than the minimum bounds of the window.
  AdjustSnappedWindowBounds(&left_or_top_rect, &right_or_bottom_rect);

  if (IsCurrentScreenOrientationPrimary())
    return (snap_position == LEFT) ? left_or_top_rect : right_or_bottom_rect;
  else
    return (snap_position == LEFT) ? right_or_bottom_rect : left_or_top_rect;
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInScreenUnadjusted(
    aura::Window* window,
    SnapPosition snap_position) {
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window);
  if (snap_position == NONE)
    return work_area_bounds_in_screen;

  gfx::Rect left_or_top_rect, right_or_bottom_rect;
  GetSnappedWindowBoundsInScreenInternal(window, &left_or_top_rect,
                                         &right_or_bottom_rect);

  if (IsCurrentScreenOrientationPrimary())
    return (snap_position == LEFT) ? left_or_top_rect : right_or_bottom_rect;
  else
    return (snap_position == LEFT) ? right_or_bottom_rect : left_or_top_rect;
}

int SplitViewController::GetDefaultDividerPosition(aura::Window* window) const {
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window);
  gfx::Size divider_size;
  // In clamshell mode, there is no divider bar, thus divider_size is empty.
  if (split_view_type_ == SplitViewType::kTabletType) {
    divider_size = SplitViewDivider::GetDividerSize(
        work_area_bounds_in_screen, GetCurrentScreenOrientation(),
        false /* is_dragging */);
  }
  if (IsCurrentScreenOrientationLandscape())
    return (work_area_bounds_in_screen.width() - divider_size.width()) * 0.5f;
  else
    return (work_area_bounds_in_screen.height() - divider_size.height()) * 0.5f;
}

void SplitViewController::StartResize(const gfx::Point& location_in_screen) {
  DCHECK(IsSplitViewModeActive());

  // |is_resizing_| may be true here, because you can start dragging the divider
  // with a pointing device while already dragging it by touch, or vice versa.
  // It is possible by using the emulator or chrome://flags/#force-tablet-mode.
  // Bailing out here does not stop the user from dragging by touch and with a
  // pointing device simultaneously; it just avoids duplicate calls to
  // CreateDragDetails() and OnDragStarted(). We also bail out here if you try
  // to start dragging the divider during its snap animation.
  if (is_resizing_ || IsDividerAnimating())
    return;

  is_resizing_ = true;
  split_view_divider_->UpdateDividerBounds();
  previous_event_location_ = location_in_screen;

  for (auto* window : {left_window_, right_window_}) {
    if (window == nullptr)
      continue;
    wm::WindowState* window_state = wm::GetWindowState(window);
    gfx::Point location_in_parent(location_in_screen);
    ::wm::ConvertPointFromScreen(window->parent(), &location_in_parent);
    int window_component = GetWindowComponentForResize(window);
    window_state->CreateDragDetails(location_in_parent, window_component,
                                    ::wm::WINDOW_MOVE_SOURCE_TOUCH);
    window_state->OnDragStarted(window_component);
  }

  base::RecordAction(base::UserMetricsAction("SplitView_ResizeWindows"));
  if (state_ == BOTH_SNAPPED) {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        split_view_divider_->divider_widget()->GetCompositor(),
        kSplitViewResizeMultiHistogram,
        kSplitViewResizeMultiMaxLatencyHistogram);
  } else if (GetOverviewSession() && !GetOverviewSession()->IsEmpty()) {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        split_view_divider_->divider_widget()->GetCompositor(),
        kSplitViewResizeWithOverviewHistogram,
        kSplitViewResizeWithOverviewMaxLatencyHistogram);
  } else {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        split_view_divider_->divider_widget()->GetCompositor(),
        kSplitViewResizeSingleHistogram,
        kSplitViewResizeSingleMaxLatencyHistogram);
  }
}

void SplitViewController::Resize(const gfx::Point& location_in_screen) {
  DCHECK(IsSplitViewModeActive());

  if (!is_resizing_)
    return;
  presentation_time_recorder_->RequestNext();
  const gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          GetDefaultSnappedWindow());
  gfx::Point modified_location_in_screen =
      GetBoundedPosition(location_in_screen, work_area_bounds);

  // Update |divider_position_|.
  UpdateDividerPosition(modified_location_in_screen);
  NotifyDividerPositionChanged();

  // Update the black scrim layer's bounds and opacity.
  UpdateBlackScrim(modified_location_in_screen);

  // Update the snapped window/windows and divider's position.
  UpdateSnappedWindowsAndDividerBounds();

  // Apply window transform if necessary.
  SetWindowsTransformDuringResizing();

  previous_event_location_ = modified_location_in_screen;
}

void SplitViewController::EndResize(const gfx::Point& location_in_screen) {
  presentation_time_recorder_.reset();
  DCHECK(IsSplitViewModeActive());
  if (!is_resizing_)
    return;
  // TODO(xdai): Use fade out animation instead of just removing it.
  black_scrim_layer_.reset();
  is_resizing_ = false;

  const gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          GetDefaultSnappedWindow());
  gfx::Point modified_location_in_screen =
      GetBoundedPosition(location_in_screen, work_area_bounds);
  UpdateDividerPosition(modified_location_in_screen);
  NotifyDividerPositionChanged();
  // Need to update snapped windows bounds even if the split view mode may have
  // to exit. Otherwise it's possible for a snapped window stuck in the edge of
  // of the screen while overview mode is active.
  UpdateSnappedWindowsAndDividerBounds();

  const int target_divider_position = GetClosestFixedDividerPosition();
  if (divider_position_ == target_divider_position) {
    EndResizeImpl();
    EndSplitViewAfterResizingIfAppropriate();
  } else {
    divider_snap_animation_ = std::make_unique<DividerSnapAnimation>(
        this, divider_position_, target_divider_position);
    divider_snap_animation_->Show();
  }
}

void SplitViewController::ShowAppCannotSnapToast() {
  ash::ToastData toast(
      kAppCannotSnapToastId,
      l10n_util::GetStringUTF16(IDS_ASH_SPLIT_VIEW_CANNOT_SNAP),
      kAppCannotSnapToastDurationMs, base::Optional<base::string16>());
  ash::Shell::Get()->toast_manager()->Show(toast);
}

void SplitViewController::EndSplitView(EndReason end_reason) {
  if (!IsSplitViewModeActive())
    return;

  end_reason_ = end_reason;

  // If we are currently in a resize but split view is ending, make sure to end
  // the resize. This can happen, for example, on the transition back to
  // clamshell mode or when a task is minimized during a resize. Likewise, if
  // split view is ending during the divider snap animation, then clean that up.
  const bool is_divider_animating = IsDividerAnimating();
  if (is_resizing_ || is_divider_animating) {
    is_resizing_ = false;
    if (is_divider_animating)
      StopAndShoveAnimatedDivider();
    EndResizeImpl();
  }

  // Remove observers when the split view mode ends.
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->overview_controller()->RemoveObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);

  StopObserving(LEFT);
  StopObserving(RIGHT);
  split_view_divider_.reset();
  black_scrim_layer_.reset();
  default_snap_position_ = NONE;
  divider_position_ = -1;
  divider_closest_ratio_ = 0.f;
  snapping_window_transformed_bounds_map_.clear();

  UpdateSplitViewStateAndNotifyObservers();
  base::RecordAction(base::UserMetricsAction("SplitView_EndSplitView"));
  UMA_HISTOGRAM_LONG_TIMES("Ash.SplitView.TimeInSplitView",
                           base::Time::Now() - splitview_start_time_);
}

bool SplitViewController::IsWindowInSplitView(
    const aura::Window* window) const {
  return window && (window == left_window_ || window == right_window_);
}

void SplitViewController::OnWindowDragStarted(aura::Window* dragged_window) {
  DCHECK(dragged_window);
  if (IsWindowInSplitView(dragged_window))
    OnSnappedWindowDetached(dragged_window, /*window_drag=*/true);

  // OnSnappedWindowDetached() may end split view mode.
  if (split_view_divider_)
    split_view_divider_->OnWindowDragStarted(dragged_window);
}

void SplitViewController::OnWindowDragEnded(
    aura::Window* dragged_window,
    SnapPosition desired_snap_position,
    const gfx::Point& last_location_in_screen) {
  if (wm::IsDraggingTabs(dragged_window)) {
    dragged_window->SetProperty(
        kTabDroppedWindowStateTypeKey,
        GetStateTypeFromSnapPosition(desired_snap_position));
    dragged_window_observer_.reset(new TabDraggedWindowObserver(
        this, dragged_window, desired_snap_position, last_location_in_screen));
  } else {
    EndWindowDragImpl(dragged_window, /*is_being_destroyed=*/false,
                      desired_snap_position, last_location_in_screen);
  }
}

void SplitViewController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SplitViewController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SplitViewController::FlushForTesting() {
  mojo_observers_.FlushForTesting();
  bindings_.FlushForTesting();
}

void SplitViewController::AddObserver(mojom::SplitViewObserverPtr observer) {
  mojom::SplitViewObserver* observer_ptr = observer.get();
  mojo_observers_.AddPtr(std::move(observer));
  observer_ptr->OnSplitViewStateChanged(ToMojomSplitViewState(state_));
}

void SplitViewController::OnWindowDestroyed(aura::Window* window) {
  DCHECK(IsSplitViewModeActive());
  DCHECK(IsWindowInSplitView(window));
  auto iter = snapping_window_transformed_bounds_map_.find(window);
  if (iter != snapping_window_transformed_bounds_map_.end())
    snapping_window_transformed_bounds_map_.erase(iter);
  OnSnappedWindowDetached(window, /*window_drag=*/false);
}

void SplitViewController::OnWindowPropertyChanged(aura::Window* window,
                                                  const void* key,
                                                  intptr_t old) {
  // If the window's resizibility property changes (must from resizable ->
  // unresizable), end the split view mode and also end overview mode if
  // overview mode is active at the moment.
  if (key == aura::client::kResizeBehaviorKey && !CanSnapInSplitview(window)) {
    EndSplitView();
    EndOverview();
    ShowAppCannotSnapToast();
  }
}

void SplitViewController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (!IsSplitViewModeActive())
    return;

  if (split_view_type_ == SplitViewType::kClamshellType &&
      reason == ui::PropertyChangeReason::NOT_FROM_ANIMATION) {
    const gfx::Rect work_area =
        screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
            window);
    const bool is_left_or_top_window =
        IsCurrentScreenOrientationPrimary()
            ? (window == left_window_ ? true : false)
            : (window == left_window_ ? false : true);
    if (IsCurrentScreenOrientationLandscape()) {
      divider_position_ = is_left_or_top_window
                              ? new_bounds.width()
                              : work_area.width() - new_bounds.width();
    } else {
      divider_position_ = is_left_or_top_window
                              ? new_bounds.height()
                              : work_area.height() - new_bounds.height();
    }
    NotifyDividerPositionChanged();
  }
}

void SplitViewController::OnResizeLoopStarted(aura::Window* window) {
  if (split_view_type_ != SplitViewType::kClamshellType ||
      !IsSplitViewModeActive()) {
    return;
  }

  const int resize_component =
      wm::GetWindowState(window)->drag_details()->window_component;
  const OrientationLockType orientation_type = GetCurrentScreenOrientation();

  // In clamshell mode, if splitview is active (which means overview is active
  // at the same time), only the resize that happens on the window edge that's
  // next to the overview grid will resize the window and overview grid at the
  // same time. For the resize that happens on the other part of the window,
  // we'll just end splitview and overview mode.
  bool should_end_splitview = true;
  switch (orientation_type) {
    case OrientationLockType::kLandscapePrimary:
      should_end_splitview =
          !(window == left_window_ && resize_component == HTRIGHT) &&
          !(window == right_window_ && resize_component == HTLEFT);
      break;
    case OrientationLockType::kLandscapeSecondary:
      should_end_splitview =
          !(window == left_window_ && resize_component == HTLEFT) &&
          !(window == right_window_ && resize_component == HTRIGHT);
      break;
    case OrientationLockType::kPortraitPrimary:
      should_end_splitview =
          !(window == left_window_ && resize_component == HTBOTTOM) &&
          !(window == right_window_ && resize_component == HTTOP);
      break;
    case OrientationLockType::kPortraitSecondary:
      should_end_splitview =
          !(window == left_window_ && resize_component == HTTOP) &&
          !(window == right_window_ && resize_component == HTBOTTOM);
      break;
    default:
      break;
  }

  if (should_end_splitview) {
    EndSplitView();
    EndOverview();
  }
}

void SplitViewController::OnResizeLoopEnded(aura::Window* window) {
  if (split_view_type_ != SplitViewType::kClamshellType ||
      !IsSplitViewModeActive()) {
    return;
  }

  if (divider_position_ < GetDividerEndPosition() * kOneThirdPositionRatio ||
      divider_position_ > GetDividerEndPosition() * kTwoThirdPositionRatio) {
    EndSplitView();
    EndOverview();
    wm::GetWindowState(window)->Maximize();
  }
}

void SplitViewController::OnPostWindowStateTypeChange(
    ash::wm::WindowState* window_state,
    ash::mojom::WindowStateType old_type) {
  if (window_state->IsSnapped()) {
    OnWindowSnapped(window_state->window());
  } else if (window_state->IsFullscreen() || window_state->IsMaximized()) {
    // End split view mode if one of the snapped windows gets maximized /
    // full-screened. Also end overview mode if overview mode is active at the
    // moment.
    EndSplitView();
    EndOverview();
  } else if (window_state->IsMinimized()) {
    OnSnappedWindowDetached(window_state->window(), /*window_drag=*/false);
    // Insert the minimized window back to overview if split view mode is ended
    // because of the minimization of the window, but overview mode is still
    // active at the moment.
    if (!IsSplitViewModeActive())
      InsertWindowToOverview(window_state->window());
  }
}

void SplitViewController::OnWindowActivated(ActivationReason reason,
                                            aura::Window* gained_active,
                                            aura::Window* lost_active) {
  // This may be called while SnapWindow is still underway because SnapWindow
  // will end the overview start animations which will cause the overview focus
  // window to be activated.
  aura::Window* overview_focus_window =
      GetOverviewSession() ? GetOverviewSession()->GetOverviewFocusWindow()
                           : nullptr;
  DCHECK(IsSplitViewModeActive() ||
         (overview_focus_window && overview_focus_window == gained_active));

  // If |gained_active| was activated as a side effect of a window disposition
  // change, do nothing. For example, when a snapped window is closed, another
  // window will be activated before OnWindowDestroying() is called. We should
  // not try to snap another window in this case.
  if (reason == ActivationReason::WINDOW_DISPOSITION_CHANGED)
    return;

  // Only snap window that hasn't been snapped.
  if (!gained_active || gained_active == left_window_ ||
      gained_active == right_window_) {
    return;
  }

  // Do not snap the window if the activation change is caused by dragging a
  // window, or by dragging a tab. Note the two values WindowState::is_dragged()
  // and IsDraggingTabs() might not be exactly the same under certain
  // circumstance, e.g., when a tab is dragged out from a browser window, a new
  // browser window will be created for the dragged tab and then be activated,
  // and at that time, IsDraggingTabs() is true, but WindowState::is_dragged()
  // is still false. And later after the window drag starts,
  // WindowState::is_dragged() will then be true.
  if (wm::GetWindowState(gained_active)->is_dragged() ||
      wm::IsDraggingTabs(gained_active)) {
    return;
  }

  // Only windows in MRU list can be snapped.
  if (!base::ContainsValue(
          Shell::Get()->mru_window_tracker()->BuildMruWindowList(),
          gained_active)) {
    return;
  }

  // If it's a user positionable window but can't be snapped, end split view
  // mode and show the cannot snap toast.
  if (!CanSnapInSplitview(gained_active)) {
    if (wm::GetWindowState(gained_active)->IsUserPositionable()) {
      EndSplitView(EndReason::kUnsnappableWindowActivated);
      ShowAppCannotSnapToast();
    }
    return;
  }

  // Snap the window on the non-default side of the screen if split view mode
  // is active.
  SnapWindow(gained_active, (default_snap_position_ == LEFT) ? RIGHT : LEFT);
}

void SplitViewController::OnPinnedStateChanged(aura::Window* pinned_window) {
  // Disable split view for pinned windows.
  if (wm::GetWindowState(pinned_window)->IsPinned() && IsSplitViewModeActive())
    EndSplitView(EndReason::kUnsnappableWindowActivated);
}

void SplitViewController::OnOverviewModeStarting() {
  DCHECK(IsSplitViewModeActive());

  // If split view mode is active, reset |state_| to make it be able to select
  // another window from overview window grid.
  if (default_snap_position_ == LEFT) {
    StopObserving(RIGHT);
  } else if (default_snap_position_ == RIGHT) {
    StopObserving(LEFT);
  }
  UpdateSplitViewStateAndNotifyObservers();
}

void SplitViewController::OnOverviewModeEnding(
    OverviewSession* overview_session) {
  DCHECK(IsSplitViewModeActive());

  // Early exit if overview is ended while swiping up on the shelf to avoid
  // snapping a window or showing a toast.
  if (overview_session->enter_exit_overview_type() ==
      OverviewSession::EnterExitOverviewType::kSwipeFromShelf) {
    EndSplitView();
    return;
  }
  aura::Window* root_window = GetDefaultSnappedWindow()->GetRootWindow();

  if (state_ == BOTH_SNAPPED) {
    // If overview is ended because of the window gets snapped, do not do
    // exiting overview animation.
    overview_session->SetWindowListNotAnimatedWhenExiting(root_window);
    return;
  }

  OverviewGrid* current_grid =
      overview_session->GetGridWithRootWindow(root_window);
  if (!current_grid || current_grid->empty()) {
    // If overview is ended with an empty overview grid, end split view as well
    // if we're in clamshell splitview mode.
    if (InClamshellSplitViewMode())
      EndSplitView();
    return;
  }

  // If split view mode is active but only has one snapped window when overview
  // mode is ending, retrieve the first snappable window in the overview window
  // grid and snap it.
  const auto& windows = current_grid->window_list();
  if (windows.size() > 0) {
    for (const auto& overview_item : windows) {
      aura::Window* window = overview_item->GetWindow();
      if (CanSnapInSplitview(window) && window != GetDefaultSnappedWindow()) {
        // Remove the overview item before snapping because the overview session
        // is unavailable to retrieve outside this function after
        // OnOverviewEnding is notified.
        overview_item->RestoreWindow(/*reset_transform=*/false);
        overview_session->RemoveItem(overview_item.get());
        SnapWindow(window, (default_snap_position_ == LEFT) ? RIGHT : LEFT);
        // If ending overview causes a window to snap, also do not do exiting
        // overview animation.
        overview_session->SetWindowListNotAnimatedWhenExiting(root_window);
        return;
      }
    }

    // Arriving here we know there is no window in the window grid can be
    // snapped, in this case end the splitview mode and show cannot snap
    // toast.
    EndSplitView();
    ShowAppCannotSnapToast();
  }
}

void SplitViewController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (!display.IsInternal())
    return;

  // We need update |previous_screen_orientation_| even though split view mode
  // is not active at the moment.
  OrientationLockType previous_screen_orientation =
      previous_screen_orientation_;
  previous_screen_orientation_ = GetCurrentScreenOrientation();

  if (!IsSplitViewModeActive())
    return;

  display::Display current_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          GetDefaultSnappedWindow());
  if (display.id() != current_display.id())
    return;

  // If one of the snapped windows becomes unsnappable, end the split view mode
  // directly.
  if ((left_window_ && !CanSnapInSplitview(left_window_)) ||
      (right_window_ && !CanSnapInSplitview(right_window_))) {
    if (!Shell::Get()->session_controller()->IsUserSessionBlocked())
      EndSplitView();
    return;
  }

  // Before adjusting the divider position for the new display metrics, if the
  // divider is animating to a snap position, then stop it and shove it there.
  // Postpone EndSplitViewAfterResizingIfAppropriate() until after the
  // adjustment, because the new display metrics will be used to compare the
  // divider position against the edges of the screen.
  if (IsDividerAnimating()) {
    StopAndShoveAnimatedDivider();
    EndResizeImpl();
  }

  if ((metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION) ||
      (metrics & display::DisplayObserver::DISPLAY_METRIC_WORK_AREA)) {
    const gfx::Size divider_size = SplitViewDivider::GetDividerSize(
        display.work_area(), GetCurrentScreenOrientation(), false);
    const int divider_thickness =
        std::min(divider_size.width(), divider_size.height());
    // Set default |divider_closest_ratio_| to kFixedPositionRatios[1].
    if (!divider_closest_ratio_)
      divider_closest_ratio_ = kFixedPositionRatios[1];

    // Reverse the position ratio if top/left window changes.
    if (IsPrimaryOrientation(previous_screen_orientation) !=
        IsCurrentScreenOrientationPrimary()) {
      divider_closest_ratio_ = 1.f - divider_closest_ratio_;
    }
    divider_position_ =
        std::floor(divider_closest_ratio_ * GetDividerEndPosition()) -
        std::floor(divider_thickness / 2.f);
  }

  // For other display configuration changes, we only move the divider to the
  // closest fixed position.
  if (!is_resizing_)
    divider_position_ = GetClosestFixedDividerPosition();

  EndSplitViewAfterResizingIfAppropriate();
  if (!IsSplitViewModeActive())
    return;

  NotifyDividerPositionChanged();
  UpdateSnappedWindowsAndDividerBounds();
}

void SplitViewController::OnTabletModeStarted() {
  split_view_type_ = SplitViewType::kTabletType;
}

void SplitViewController::OnTabletModeEnding() {
  if (IsClamshellSplitViewModeEnabled())
    split_view_type_ = SplitViewType::kClamshellType;
  EndSplitView();
}

void SplitViewController::OnTabletControllerDestroyed() {
  tablet_mode_observer_.RemoveAll();
}

void SplitViewController::OnAccessibilityStatusChanged() {
  // TODO(crubg.com/853588): Exit split screen if ChromeVox is turned on until
  // they are compatible.
  if (Shell::Get()->accessibility_controller()->spoken_feedback_enabled())
    EndSplitView();
}

void SplitViewController::StartObserving(aura::Window* window) {
  if (window && !window->HasObserver(this)) {
    Shell::Get()->shadow_controller()->UpdateShadowForWindow(window);
    window->AddObserver(this);
    wm::GetWindowState(window)->AddObserver(this);
    if (split_view_divider_)
      split_view_divider_->AddObservedWindow(window);
  }
}

void SplitViewController::StopObserving(SnapPosition snap_position) {
  aura::Window* window = snap_position == LEFT ? left_window_ : right_window_;
  if (window == left_window_)
    left_window_ = nullptr;
  else
    right_window_ = nullptr;

  if (window && window->HasObserver(this)) {
    window->RemoveObserver(this);
    wm::GetWindowState(window)->RemoveObserver(this);
    if (split_view_divider_)
      split_view_divider_->RemoveObservedWindow(window);
    Shell::Get()->shadow_controller()->UpdateShadowForWindow(window);
  }
}

void SplitViewController::UpdateSplitViewStateAndNotifyObservers() {
  State previous_state = state_;
  if (IsSnapped(left_window_) && IsSnapped(right_window_))
    state_ = BOTH_SNAPPED;
  else if (IsSnapped(left_window_))
    state_ = LEFT_SNAPPED;
  else if (IsSnapped(right_window_))
    state_ = RIGHT_SNAPPED;
  else
    state_ = NO_SNAP;

  // We still notify observers even if |state_| doesn't change as it's possible
  // to snap a window to a position that already has a snapped window.
  for (Observer& observer : observers_)
    observer.OnSplitViewStateChanged(previous_state, state_);
  mojo_observers_.ForAllPtrs([this](mojom::SplitViewObserver* observer) {
    observer->OnSplitViewStateChanged(ToMojomSplitViewState(state_));
  });

  if (previous_state != state_) {
    if (previous_state == NO_SNAP)
      Shell::Get()->NotifySplitViewModeStarted();
    else if (state_ == NO_SNAP)
      Shell::Get()->NotifySplitViewModeEnded();
  }
}

void SplitViewController::NotifyDividerPositionChanged() {
  for (Observer& observer : observers_)
    observer.OnSplitViewDividerPositionChanged();
}

void SplitViewController::UpdateBlackScrim(
    const gfx::Point& location_in_screen) {
  DCHECK(IsSplitViewModeActive());

  if (!black_scrim_layer_) {
    // Create an invisible black scrim layer.
    black_scrim_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
    black_scrim_layer_->SetColor(SK_ColorBLACK);
    GetDefaultSnappedWindow()->GetRootWindow()->layer()->Add(
        black_scrim_layer_.get());
    GetDefaultSnappedWindow()->GetRootWindow()->layer()->StackAtTop(
        black_scrim_layer_.get());
  }

  // Decide where the black scrim should show and update its bounds.
  SnapPosition position = GetBlackScrimPosition(location_in_screen);
  if (position == NONE) {
    black_scrim_layer_.reset();
    return;
  }
  black_scrim_layer_->SetBounds(
      GetSnappedWindowBoundsInScreen(GetDefaultSnappedWindow(), position));

  // Update its opacity. The opacity increases as it gets closer to the edge of
  // the screen.
  const int location = IsCurrentScreenOrientationLandscape()
                           ? location_in_screen.x()
                           : location_in_screen.y();
  gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          GetDefaultSnappedWindow());
  if (!IsCurrentScreenOrientationLandscape())
    work_area_bounds.Transpose();
  float opacity = kBlackScrimOpacity;
  const float ratio = kOneThirdPositionRatio - kBlackScrimFadeInRatio;
  const int distance = std::min(std::abs(location - work_area_bounds.x()),
                                std::abs(work_area_bounds.right() - location));
  if (distance > work_area_bounds.width() * ratio) {
    opacity -= kBlackScrimOpacity *
               (distance - work_area_bounds.width() * ratio) /
               (work_area_bounds.width() * kBlackScrimFadeInRatio);
    opacity = std::max(opacity, 0.f);
  }
  black_scrim_layer_->SetOpacity(opacity);
}

void SplitViewController::UpdateSnappedWindowsAndDividerBounds() {
  DCHECK(IsSplitViewModeActive());

  // Update the snapped windows' bounds.
  if (IsSnapped(left_window_)) {
    const wm::WMEvent left_window_event(wm::WM_EVENT_SNAP_LEFT);
    wm::GetWindowState(left_window_)->OnWMEvent(&left_window_event);
  }
  if (IsSnapped(right_window_)) {
    const wm::WMEvent right_window_event(wm::WM_EVENT_SNAP_RIGHT);
    wm::GetWindowState(right_window_)->OnWMEvent(&right_window_event);
  }

  // Update divider's bounds.
  if (split_view_divider_)
    split_view_divider_->UpdateDividerBounds();
}

SplitViewController::SnapPosition SplitViewController::GetBlackScrimPosition(
    const gfx::Point& location_in_screen) {
  const gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          GetDefaultSnappedWindow());
  if (!work_area_bounds.Contains(location_in_screen))
    return NONE;

  gfx::Size left_window_min_size, right_window_min_size;
  if (left_window_ && left_window_->delegate())
    left_window_min_size = left_window_->delegate()->GetMinimumSize();
  if (right_window_ && right_window_->delegate())
    right_window_min_size = right_window_->delegate()->GetMinimumSize();

  bool is_primary = IsCurrentScreenOrientationPrimary();
  int divider_end_position = GetDividerEndPosition();
  // The distance from the current resizing position to the left or right side
  // of the screen. Note: left or right side here means the side of the
  // |left_window_| or |right_window_|.
  int left_window_distance = 0, right_window_distance = 0;
  int min_left_length = 0, min_right_length = 0;

  if (IsCurrentScreenOrientationLandscape()) {
    int left_distance = location_in_screen.x() - work_area_bounds.x();
    int right_distance = work_area_bounds.right() - location_in_screen.x();
    left_window_distance = is_primary ? left_distance : right_distance;
    right_window_distance = is_primary ? right_distance : left_distance;

    min_left_length = left_window_min_size.width();
    min_right_length = right_window_min_size.width();
  } else {
    int top_distance = location_in_screen.y() - work_area_bounds.y();
    int bottom_distance = work_area_bounds.bottom() - location_in_screen.y();
    left_window_distance = is_primary ? top_distance : bottom_distance;
    right_window_distance = is_primary ? bottom_distance : top_distance;

    min_left_length = left_window_min_size.height();
    min_right_length = right_window_min_size.height();
  }

  if (left_window_distance < divider_end_position * kOneThirdPositionRatio ||
      left_window_distance < min_left_length) {
    return LEFT;
  }
  if (right_window_distance < divider_end_position * kOneThirdPositionRatio ||
      right_window_distance < min_right_length) {
    return RIGHT;
  }

  return NONE;
}

void SplitViewController::UpdateDividerPosition(
    const gfx::Point& location_in_screen) {
  if (IsCurrentScreenOrientationLandscape()) {
    divider_position_ += location_in_screen.x() - previous_event_location_.x();
  } else {
    divider_position_ += location_in_screen.y() - previous_event_location_.y();
  }
  divider_position_ = std::max(0, divider_position_);
}

void SplitViewController::GetSnappedWindowBoundsInScreenInternal(
    aura::Window* window,
    gfx::Rect* left_or_top_rect,
    gfx::Rect* right_or_bottom_rect) {
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window);

  // |divide_position_| might not be properly initialized yet.
  divider_position_ = (divider_position_ < 0)
                          ? GetDefaultDividerPosition(window)
                          : divider_position_;
  gfx::Rect divider_bounds;
  if (split_view_type_ == SplitViewType::kTabletType) {
    divider_bounds = SplitViewDivider::GetDividerBoundsInScreen(
        work_area_bounds_in_screen, GetCurrentScreenOrientation(),
        divider_position_, false /* is_dragging */);
  } else {
    if (IsCurrentScreenOrientationLandscape())
      divider_bounds.set_x(work_area_bounds_in_screen.x() + divider_position_);
    else
      divider_bounds.set_y(work_area_bounds_in_screen.y() + divider_position_);
  }

  SplitRect(work_area_bounds_in_screen, divider_bounds,
            IsCurrentScreenOrientationLandscape(), left_or_top_rect,
            right_or_bottom_rect);
}

void SplitViewController::SplitRect(const gfx::Rect& work_area_rect,
                                    const gfx::Rect& divider_rect,
                                    const bool is_split_vertically,
                                    gfx::Rect* left_or_top_rect,
                                    gfx::Rect* right_or_bottom_rect) {
  if (is_split_vertically) {
    left_or_top_rect->SetRect(work_area_rect.x(), work_area_rect.y(),
                              divider_rect.x() - work_area_rect.x(),
                              work_area_rect.height());
    right_or_bottom_rect->SetRect(divider_rect.right(), work_area_rect.y(),
                                  work_area_rect.width() -
                                      left_or_top_rect->width() -
                                      divider_rect.width(),
                                  work_area_rect.height());
  } else {
    left_or_top_rect->SetRect(work_area_rect.x(), work_area_rect.y(),
                              work_area_rect.width(),
                              divider_rect.y() - work_area_rect.y());
    right_or_bottom_rect->SetRect(
        work_area_rect.x(), divider_rect.bottom(), work_area_rect.width(),
        work_area_rect.height() - left_or_top_rect->height() -
            divider_rect.height());
  }
}

int SplitViewController::GetClosestFixedDividerPosition() {
  DCHECK(IsSplitViewModeActive());

  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          GetDefaultSnappedWindow());
  const gfx::Size divider_size = SplitViewDivider::GetDividerSize(
      work_area_bounds_in_screen, GetCurrentScreenOrientation(),
      false /* is_dragging */);
  const int divider_thickness =
      std::min(divider_size.width(), divider_size.height());

  // The values in |kFixedPositionRatios| represent the fixed position of the
  // center of the divider while |divider_position_| represent the origin of the
  // divider rectangle. So, before calling FindClosestFixedPositionRatio,
  // extract the center from |divider_position_|. The result will also be the
  // center of the divider, so extract the origin, unless the result is on of
  // the endpoints.
  int divider_end_position = GetDividerEndPosition();
  divider_closest_ratio_ = FindClosestPositionRatio(
      divider_position_ + std::floor(divider_thickness / 2.f),
      divider_end_position);
  int fix_position = std::floor(divider_end_position * divider_closest_ratio_);
  if (divider_closest_ratio_ > 0.f && divider_closest_ratio_ < 1.f)
    fix_position -= std::floor(divider_thickness / 2.f);
  return fix_position;
}

bool SplitViewController::IsDividerAnimating() {
  return divider_snap_animation_ && divider_snap_animation_->is_animating();
}

void SplitViewController::StopAndShoveAnimatedDivider() {
  DCHECK(IsDividerAnimating());

  divider_snap_animation_->Stop();
  divider_position_ = divider_snap_animation_->ending_position();
  NotifyDividerPositionChanged();
  UpdateSnappedWindowsAndDividerBounds();
}

bool SplitViewController::ShouldEndSplitViewAfterResizing() {
  DCHECK(IsSplitViewModeActive());

  return divider_position_ == 0 || divider_position_ == GetDividerEndPosition();
}

void SplitViewController::EndSplitViewAfterResizingIfAppropriate() {
  if (!ShouldEndSplitViewAfterResizing())
    return;

  aura::Window* active_window = GetActiveWindowAfterResizingUponExit();

  // Track the window that needs to be put back into the overview list if we
  // remain in overview mode.
  aura::Window* insert_overview_window = nullptr;
  if (Shell::Get()->overview_controller()->IsSelecting())
    insert_overview_window = GetDefaultSnappedWindow();
  EndSplitView();
  if (active_window) {
    EndOverview();
    wm::ActivateWindow(active_window);
  } else if (insert_overview_window) {
    // The dimensions of |window| will be very slim because of dragging the
    // divider to the edge. Change the window dimensions to its tablet mode
    // dimensions. Note: if split view is no longer constrained to tablet mode
    // this will be need to updated.
    TabletModeWindowState::UpdateWindowPosition(
        wm::GetWindowState(insert_overview_window), /*animate=*/false);
    InsertWindowToOverview(insert_overview_window, /*animate=*/false);
  }
}

aura::Window* SplitViewController::GetActiveWindowAfterResizingUponExit() {
  DCHECK(IsSplitViewModeActive());

  if (!ShouldEndSplitViewAfterResizing())
    return nullptr;

  if (divider_position_ == 0) {
    return IsCurrentScreenOrientationPrimary() ? right_window_ : left_window_;
  } else {
    return IsCurrentScreenOrientationPrimary() ? left_window_ : right_window_;
  }
}

int SplitViewController::GetDividerEndPosition() {
  const gfx::Rect work_area_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          GetDefaultSnappedWindow());
  return IsCurrentScreenOrientationLandscape() ? work_area_bounds.width()
                                               : work_area_bounds.height();
}

void SplitViewController::OnWindowSnapped(aura::Window* window) {
  RestoreTransformIfApplicable(window);
  UpdateSplitViewStateAndNotifyObservers();
  ActivateAndStackSnappedWindow(window);

  // If there are two window snapped in clamshell mode, splitview mode is ended.
  if (state_ == BOTH_SNAPPED &&
      split_view_type_ == SplitViewType::kClamshellType) {
    EndSplitView();
  }
}

void SplitViewController::OnSnappedWindowDetached(aura::Window* window,
                                                  bool window_drag) {
  DCHECK(IsWindowInSplitView(window));
  if (left_window_ == window) {
    StopObserving(LEFT);
  } else {
    StopObserving(RIGHT);
  }

  // Resizing (or the divider snap animation) may continue, but |window| will no
  // longer have anything to do with it.
  if (is_resizing_ || IsDividerAnimating())
    FinishWindowResizing(window);

  if (!left_window_ && !right_window_) {
    // If there is no snapped window at this moment, ends split view mode. Note
    // this will update overview window grid bounds if the overview mode is
    // active at the moment.
    EndSplitView(window_drag ? EndReason::kWindowDragStarted
                             : EndReason::kNormal);
  } else {
    // If there is still one snapped window after minimizing/closing one snapped
    // window, update its snap state and open overview window grid.
    default_snap_position_ = left_window_ ? LEFT : RIGHT;
    UpdateSplitViewStateAndNotifyObservers();
    StartOverview(window_drag);
  }
}

void SplitViewController::AdjustSnappedWindowBounds(
    gfx::Rect* left_or_top_rect,
    gfx::Rect* right_or_bottom_rect) {
  aura::Window* left_or_top_window =
      IsCurrentScreenOrientationPrimary() ? left_window_ : right_window_;
  aura::Window* right_or_bottom_window =
      IsCurrentScreenOrientationPrimary() ? right_window_ : left_window_;

  const bool is_landscape = IsCurrentScreenOrientationLandscape();
  const int left_minimum_width =
      GetMinimumWindowSize(left_or_top_window, is_landscape);
  const int right_minimum_width =
      GetMinimumWindowSize(right_or_bottom_window, is_landscape);

  if (!is_landscape) {
    left_or_top_rect->Transpose();
    right_or_bottom_rect->Transpose();
  }

  if (left_or_top_rect->width() < left_minimum_width)
    left_or_top_rect->set_width(left_minimum_width);
  if (right_or_bottom_rect->width() < right_minimum_width) {
    right_or_bottom_rect->set_x(
        right_or_bottom_rect->x() -
        (right_minimum_width - right_or_bottom_rect->width()));
    right_or_bottom_rect->set_width(right_minimum_width);
  }

  if (!is_landscape) {
    left_or_top_rect->Transpose();
    right_or_bottom_rect->Transpose();
  }
}

float SplitViewController::FindClosestPositionRatio(float distance,
                                                    float length) {
  float current_ratio = distance / length;
  float closest_ratio = 0.f;
  std::vector<float> position_ratios(
      kFixedPositionRatios,
      kFixedPositionRatios + sizeof(kFixedPositionRatios) / sizeof(float));
  GetDividerOptionalPositionRatios(&position_ratios);
  for (float ratio : position_ratios) {
    if (std::abs(current_ratio - ratio) <
        std::abs(current_ratio - closest_ratio)) {
      closest_ratio = ratio;
    }
  }
  return closest_ratio;
}

void SplitViewController::GetDividerOptionalPositionRatios(
    std::vector<float>* out_position_ratios) {
  bool is_left_or_top = IsCurrentScreenOrientationPrimary();
  aura::Window* left_or_top_window =
      is_left_or_top ? left_window_ : right_window_;
  aura::Window* right_or_bottom_window =
      is_left_or_top ? right_window_ : left_window_;
  bool is_landscape = IsCurrentScreenOrientationLandscape();

  float min_size_left_ratio = 0.f, min_size_right_ratio = 0.f;
  int min_left_size = 0, min_right_size = 0;
  if (left_or_top_window && left_or_top_window->delegate()) {
    gfx::Size min_size = left_or_top_window->delegate()->GetMinimumSize();
    min_left_size = is_landscape ? min_size.width() : min_size.height();
  }
  if (right_or_bottom_window && right_or_bottom_window->delegate()) {
    gfx::Size min_size = right_or_bottom_window->delegate()->GetMinimumSize();
    min_right_size = is_landscape ? min_size.width() : min_size.height();
  }

  int divider_end_position = GetDividerEndPosition();
  min_size_left_ratio =
      static_cast<float>(min_left_size) / divider_end_position;
  min_size_right_ratio =
      static_cast<float>(min_right_size) / divider_end_position;
  if (min_size_left_ratio <= kOneThirdPositionRatio)
    out_position_ratios->push_back(kOneThirdPositionRatio);

  if (min_size_right_ratio <= kOneThirdPositionRatio)
    out_position_ratios->push_back(kTwoThirdPositionRatio);
}

int SplitViewController::GetWindowComponentForResize(aura::Window* window) {
  if (IsWindowInSplitView(window)) {
    switch (GetCurrentScreenOrientation()) {
      case OrientationLockType::kLandscapePrimary:
        return (window == left_window_) ? HTRIGHT : HTLEFT;
      case OrientationLockType::kLandscapeSecondary:
        return (window == left_window_) ? HTLEFT : HTRIGHT;
      case OrientationLockType::kPortraitSecondary:
        return (window == left_window_) ? HTTOP : HTBOTTOM;
      case OrientationLockType::kPortraitPrimary:
        return (window == left_window_) ? HTBOTTOM : HTTOP;
      default:
        return HTNOWHERE;
    }
  }
  return HTNOWHERE;
}

gfx::Point SplitViewController::GetEndDragLocationInScreen(
    aura::Window* window,
    const gfx::Point& location_in_screen) {
  gfx::Point end_location(location_in_screen);
  if (!IsWindowInSplitView(window))
    return end_location;

  const gfx::Rect bounds = (window == left_window_)
                               ? GetSnappedWindowBoundsInScreen(window, LEFT)
                               : GetSnappedWindowBoundsInScreen(window, RIGHT);
  switch (GetCurrentScreenOrientation()) {
    case OrientationLockType::kLandscapePrimary:
      end_location.set_x(window == left_window_ ? bounds.right() : bounds.x());
      break;
    case OrientationLockType::kLandscapeSecondary:
      end_location.set_x(window == left_window_ ? bounds.x() : bounds.right());
      break;
    case OrientationLockType::kPortraitSecondary:
      end_location.set_y(window == left_window_ ? bounds.y() : bounds.bottom());
      break;
    case OrientationLockType::kPortraitPrimary:
      end_location.set_y(window == left_window_ ? bounds.bottom() : bounds.y());
      break;
    default:
      NOTREACHED();
      break;
  }
  return end_location;
}

void SplitViewController::RestoreTransformIfApplicable(aura::Window* window) {
  DCHECK(IsWindowInSplitView(window));

  // If the transform of the window has been changed, calculate a good starting
  // transform based on its transformed bounds before to be snapped.
  auto iter = snapping_window_transformed_bounds_map_.find(window);
  if (iter == snapping_window_transformed_bounds_map_.end())
    return;

  const gfx::Rect item_bounds = iter->second;
  snapping_window_transformed_bounds_map_.erase(iter);

  // Restore the window's transform first if it's not identity.
  if (!window->layer()->GetTargetTransform().IsIdentity()) {
    // Calculate the starting transform based on the window's expected snapped
    // bounds and its transformed bounds before to be snapped.
    const gfx::Rect snapped_bounds = GetSnappedWindowBoundsInScreen(
        window, (window == left_window_) ? LEFT : RIGHT);
    const gfx::Transform starting_transform =
        ScopedOverviewTransformWindow::GetTransformForRect(
            gfx::RectF(snapped_bounds), gfx::RectF(item_bounds));
    SetTransformWithAnimation(window, starting_transform, gfx::Transform());
  }
}

void SplitViewController::ActivateAndStackSnappedWindow(aura::Window* window) {
  wm::ActivateWindow(window);

  // Stack the other snapped window below the current active window so that the
  // two snapped window are always the top two windows when split view mode is
  // active.
  aura::Window* stacking_target =
      (window == left_window_) ? right_window_ : left_window_;

  // Only try to restack the snapped windows if they have the same parent
  // window. Otherwise, just make sure the |stacking_target| is the top
  // child window of its parent.
  // TODO(xdai): Find better ways to handle this case.
  if (stacking_target) {
    if (stacking_target->parent() == window->parent())
      stacking_target->parent()->StackChildBelow(stacking_target, window);
    else
      stacking_target->parent()->StackChildAtTop(stacking_target);
  }
}

void SplitViewController::SetWindowsTransformDuringResizing() {
  DCHECK(IsSplitViewModeActive());
  const bool is_landscape = IsCurrentScreenOrientationLandscape();
  aura::Window* left_or_top_window =
      IsCurrentScreenOrientationPrimary() ? left_window_ : right_window_;
  aura::Window* right_or_bottom_window =
      IsCurrentScreenOrientationPrimary() ? right_window_ : left_window_;

  gfx::Rect left_or_top_rect, right_or_bottom_rect;
  GetSnappedWindowBoundsInScreenInternal(
      GetDefaultSnappedWindow(), &left_or_top_rect, &right_or_bottom_rect);

  gfx::Transform left_or_top_transform;
  if (left_or_top_window) {
    const int left_size =
        is_landscape ? left_or_top_rect.width() : left_or_top_rect.height();
    const int left_minimum_size =
        GetMinimumWindowSize(left_or_top_window, is_landscape);
    const int distance = left_size - left_minimum_size;
    if (distance < 0) {
      left_or_top_transform.Translate(is_landscape ? distance : 0,
                                      is_landscape ? 0 : distance);
    }
    SetTransform(left_or_top_window, left_or_top_transform);
  }

  gfx::Transform right_or_bottom_transform;
  if (right_or_bottom_window) {
    const int right_size = is_landscape ? right_or_bottom_rect.width()
                                        : right_or_bottom_rect.height();
    const int right_minimum_size =
        GetMinimumWindowSize(right_or_bottom_window, is_landscape);
    const int distance = right_size - right_minimum_size;
    if (distance < 0) {
      right_or_bottom_transform.Translate(is_landscape ? -distance : 0,
                                          is_landscape ? 0 : -distance);
    }
    SetTransform(right_or_bottom_window, right_or_bottom_transform);
  }

  if (black_scrim_layer_.get()) {
    black_scrim_layer_->SetTransform(left_or_top_transform.IsIdentity()
                                         ? right_or_bottom_transform
                                         : left_or_top_transform);
  }
}

void SplitViewController::RestoreWindowsTransformAfterResizing() {
  DCHECK(IsSplitViewModeActive());
  if (left_window_)
    SetTransform(left_window_, gfx::Transform());
  if (right_window_)
    SetTransform(right_window_, gfx::Transform());
  if (black_scrim_layer_.get())
    black_scrim_layer_->SetTransform(gfx::Transform());
}

void SplitViewController::SetTransformWithAnimation(
    aura::Window* window,
    const gfx::Transform& start_transform,
    const gfx::Transform& target_transform) {
  gfx::Point target_origin =
      gfx::ToRoundedPoint(GetTargetBoundsInScreen(window).origin());
  for (auto* window_iter : wm::GetTransientTreeIterator(window)) {
    // Adjust |start_transform| and |target_transform| for the transient child.
    aura::Window* parent_window = window_iter->parent();
    gfx::Rect original_bounds(window_iter->GetTargetBounds());
    ::wm::ConvertRectToScreen(parent_window, &original_bounds);
    gfx::Transform new_start_transform =
        TransformAboutPivot(gfx::Point(target_origin.x() - original_bounds.x(),
                                       target_origin.y() - original_bounds.y()),
                            start_transform);
    gfx::Transform new_target_transform =
        TransformAboutPivot(gfx::Point(target_origin.x() - original_bounds.x(),
                                       target_origin.y() - original_bounds.y()),
                            target_transform);
    if (new_start_transform != window_iter->layer()->GetTargetTransform())
      window_iter->SetTransform(new_start_transform);

    DoSplitviewTransformAnimation(window_iter->layer(),
                                  SPLITVIEW_ANIMATION_SET_WINDOW_TRANSFORM,
                                  new_target_transform);
  }
}

void SplitViewController::RemoveWindowFromOverviewIfApplicable(
    aura::Window* window) {
  if (!Shell::Get()->overview_controller()->IsSelecting())
    return;

  OverviewSession* overview_session = GetOverviewSession();
  OverviewGrid* current_grid =
      overview_session->GetGridWithRootWindow(window->GetRootWindow());
  if (!current_grid)
    return;

  OverviewItem* item = current_grid->GetOverviewItemContaining(window);
  if (!item)
    return;

  // Remove it from the grid. The transform will be reset later after the
  // window is snapped. Note the remaining windows in overview don't need to be
  // repositioned in this case as they have been positioned to the right place
  // during dragging.
  item->RestoreWindow(/*reset_transform=*/false);
  overview_session->RemoveItem(item);
}

void SplitViewController::UpdateSnappingWindowTransformedBounds(
    aura::Window* window) {
  if (!window->layer()->GetTargetTransform().IsIdentity()) {
    snapping_window_transformed_bounds_map_[window] =
        gfx::ToEnclosedRect(GetTransformedBounds(window, /*top_inset=*/0));
  }
}

void SplitViewController::InsertWindowToOverview(aura::Window* window,
                                                 bool animate) {
  if (!window || !GetOverviewSession())
    return;
  GetOverviewSession()->AddItem(window, /*reposition=*/true, animate);
}

void SplitViewController::StartOverview(bool window_drag) {
  if (!Shell::Get()->overview_controller()->IsSelecting()) {
    Shell::Get()->overview_controller()->ToggleOverview(
        window_drag ? OverviewSession::EnterExitOverviewType::kWindowDragged
                    : OverviewSession::EnterExitOverviewType::kNormal);
  }
}

void SplitViewController::EndOverview() {
  if (Shell::Get()->overview_controller()->IsSelecting())
    Shell::Get()->overview_controller()->ToggleOverview();
}

void SplitViewController::FinishWindowResizing(aura::Window* window) {
  if (window != nullptr) {
    wm::WindowState* window_state = wm::GetWindowState(window);
    window_state->OnCompleteDrag(
        GetEndDragLocationInScreen(window, previous_event_location_));
    window_state->DeleteDragDetails();
  }
}

void SplitViewController::EndResizeImpl() {
  DCHECK(IsSplitViewModeActive());
  DCHECK(!is_resizing_);
  // Resize may not end with |EndResize()|, so make sure to clear here too.
  presentation_time_recorder_.reset();
  RestoreWindowsTransformAfterResizing();
  FinishWindowResizing(left_window_);
  FinishWindowResizing(right_window_);
}

void SplitViewController::EndWindowDragImpl(
    aura::Window* window,
    bool is_being_destroyed,
    SnapPosition desired_snap_position,
    const gfx::Point& last_location_in_screen) {
  if (split_view_divider_)
    split_view_divider_->OnWindowDragEnded();

  // If the dragged window is to be destroyed, do not try to snap it.
  if (is_being_destroyed)
    return;

  // If dragged window was in overview before or it has been added to overview
  // window by dropping on the new selector item, do nothing.
  if (GetOverviewSession() && GetOverviewSession()->IsWindowInOverview(window))
    return;

  const bool was_splitview_active = IsSplitViewModeActive();
  if (desired_snap_position == SplitViewController::NONE) {
    if (was_splitview_active) {
      // Even though |snap_position| equals |NONE|, the dragged window still
      // needs to be snapped if splitview mode is active at the momemnt.
      // Calculate the expected snap position based on the last event
      // location. Note if there is already a window at |desired_snap_postion|,
      // SnapWindow() will put the previous snapped window in overview.
      SnapWindow(window, GetSnapPosition(window, last_location_in_screen));
    } else {
      // Restore the dragged window's transform first if it's not identity. It
      // needs to be called before the transformed window's bounds change so
      // that its transient children are layout'ed properly (the layout happens
      // when window's bounds change).
      SetTransformWithAnimation(window, window->layer()->GetTargetTransform(),
                                gfx::Transform());

      OverviewSession* overview_session = GetOverviewSession();
      if (overview_session) {
        overview_session->SetWindowListNotAnimatedWhenExiting(
            window->GetRootWindow());
        // Set the overview exit type to kWindowDragged to avoid update bounds
        // animation of the windows in overview grid.
        overview_session->set_enter_exit_overview_type(
            OverviewSession::EnterExitOverviewType::kWindowDragged);
      }
      // Activate the dragged window and end the overview. The dragged window
      // will be restored back to its previous state before dragging.
      wm::ActivateWindow(window);
      EndOverview();

      // Update the dragged window's bounds. It's possible that the dragged
      // window's bounds was changed during dragging. Update its bounds after
      // the drag ends to ensure it has the right bounds.
      TabletModeWindowState::UpdateWindowPosition(wm::GetWindowState(window),
                                                  /*animate=*/true);
    }
  } else {
    // Note SnapWindow() might put the previous window that was snapped at the
    // |desired_snap_position| in overview.
    SnapWindow(window, desired_snap_position);
    // Reapply the bounds update because the bounds might have been
    // modified by dragging operation.
    // TODO(oshima): WindowState already gets notified when drag ends. Refactor
    // WindowState so that each state implementation can take action when
    // drag ends.
    const wm::WMEvent event(desired_snap_position == SplitViewController::LEFT
                                ? wm::WM_EVENT_SNAP_LEFT
                                : wm::WM_EVENT_SNAP_RIGHT);
    wm::GetWindowState(window)->OnWMEvent(&event);

    if (!was_splitview_active) {
      // If splitview mode was not active before snapping the dragged
      // window, snap the initiator window to the other side of the screen
      // if it's not the same window as the dragged window.
      aura::Window* initiator_window =
          window->GetProperty(ash::kTabDraggingSourceWindowKey);
      if (initiator_window && initiator_window != window) {
        SnapWindow(initiator_window,
                   (desired_snap_position == SplitViewController::LEFT)
                       ? SplitViewController::RIGHT
                       : SplitViewController::LEFT);
      } else {
        // If overview is not active, open overview.
        StartOverview();
      }
    }
  }
}

SplitViewController::SnapPosition SplitViewController::GetSnapPosition(
    aura::Window* window,
    const gfx::Point& last_location_in_screen) {
  const int divider_position = IsSplitViewModeActive()
                                   ? this->divider_position()
                                   : GetDefaultDividerPosition(window);
  const int position = IsCurrentScreenOrientationLandscape()
                           ? last_location_in_screen.x()
                           : last_location_in_screen.y();
  return (position <= divider_position) == IsCurrentScreenOrientationPrimary()
             ? SplitViewController::LEFT
             : SplitViewController::RIGHT;
}

}  // namespace ash
