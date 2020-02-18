// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_delegate.h"

#include <vector>

#include "ash/home_screen/home_screen_controller.h"
#include "ash/public/cpp/wallpaper_types.h"
#include "ash/root_window_controller.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The scale factor that the source window should scale if the source window is
// not the dragged window && is not in splitscreen when drag starts && the user
// has dragged the window to pass the |kIndicatorThresholdRatio| vertical
// threshold.
constexpr float kSourceWindowScale = 0.85;

// Threshold of the fling velocity to keep the dragged window as a new separate
// window after drag ends and do not try to merge it back into source window.
constexpr float kFlingToStayAsNewWindowThreshold = 2000.f;

// The class to observe the source window's bounds change animation. It's used
// to prevent the dragged window to merge back into the source window during
// dragging. Only when the source window restores to its maximized window size,
// the dragged window can be merged back into the source window.
class SourceWindowAnimationObserver : public ui::ImplicitAnimationObserver,
                                      public aura::WindowObserver {
 public:
  SourceWindowAnimationObserver(aura::Window* source_window,
                                aura::Window* dragged_window)
      : source_window_(source_window), dragged_window_(dragged_window) {
    source_window_->AddObserver(this);
    dragged_window_->AddObserver(this);
  }

  ~SourceWindowAnimationObserver() override { StopObserving(); }

  // ui::ImplicitAnimationObserver:
  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override {
    DCHECK(dragged_window_ && source_window_);
    dragged_window_->SetProperty(ash::kCanAttachToAnotherWindowKey, false);
  }

  void OnImplicitAnimationsCompleted() override {
    DCHECK(dragged_window_ && source_window_);
    // When arriving here, we know the source window bounds change animation
    // just ended. Only clear the property ash::kCanAttachToAnotherWindowKey if
    // the source window bounds restores to its maximized window size.
    gfx::Rect work_area_bounds = display::Screen::GetScreen()
                                     ->GetDisplayNearestWindow(source_window_)
                                     .work_area();
    ::wm::ConvertRectFromScreen(source_window_->parent(), &work_area_bounds);
    if (source_window_->bounds() == work_area_bounds)
      StopObserving();
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK(window == source_window_ || window == dragged_window_);
    StopObserving();
  }

 private:
  void StopObserving() {
    StopObservingImplicitAnimations();
    if (source_window_) {
      source_window_->RemoveObserver(this);
      source_window_ = nullptr;
    }

    if (dragged_window_) {
      dragged_window_->RemoveObserver(this);
      dragged_window_->ClearProperty(ash::kCanAttachToAnotherWindowKey);
      dragged_window_ = nullptr;
    }
  }

  aura::Window* source_window_;
  aura::Window* dragged_window_;

  DISALLOW_COPY_AND_ASSIGN(SourceWindowAnimationObserver);
};

}  // namespace

// WindowsHider hides all visible windows except the currently dragged window
// and the dragged window's source window upon its creation, and restores the
// windows' visibility upon its destruction. It also blurs and darkens the
// background, hides the home launcher if home launcher is enabled. Only need to
// do so if we need to scale up and down the source window when dragging a tab
// window out of it.
class TabletModeBrowserWindowDragDelegate::WindowsHider
    : public aura::WindowObserver {
 public:
  explicit WindowsHider(aura::Window* dragged_window)
      : dragged_window_(dragged_window) {
    DCHECK(dragged_window);
    aura::Window* source_window =
        dragged_window->GetProperty(ash::kTabDraggingSourceWindowKey);
    DCHECK(source_window);

    // Disable the backdrop for |source_window| during dragging.
    source_window_backdrop_ = source_window->GetProperty(kBackdropWindowMode);
    source_window->SetProperty(kBackdropWindowMode,
                               BackdropWindowMode::kDisabled);

    DCHECK(!Shell::Get()->overview_controller()->InOverviewSession());

    aura::Window* root_window = dragged_window->GetRootWindow();
    std::vector<aura::Window*> windows =
        Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
    for (aura::Window* window : windows) {
      if (window == dragged_window || window == source_window ||
          window->GetRootWindow() != root_window) {
        continue;
      }

      window_visibility_map_.emplace(window, window->IsVisible());
      if (window->IsVisible()) {
        ScopedAnimationDisabler disabler(window);
        window->Hide();
      }
      window->AddObserver(this);
    }

    // Hide the home launcher if it's enabled during dragging.
    Shell::Get()->home_screen_controller()->OnWindowDragStarted();

    // Blurs the wallpaper background.
    auto* wallpaper_view = RootWindowController::ForWindow(root_window)
                               ->wallpaper_widget_controller()
                               ->wallpaper_view();
    if (wallpaper_view) {
      wallpaper_view->RepaintBlurAndOpacity(kWallpaperBlurSigma,
                                            kShieldOpacity);
    }
  }

  ~WindowsHider() override {
    // It might be possible that |source_window| is destroyed during dragging.
    aura::Window* source_window =
        dragged_window_->GetProperty(ash::kTabDraggingSourceWindowKey);
    if (source_window)
      source_window->SetProperty(kBackdropWindowMode, source_window_backdrop_);

    for (auto iter = window_visibility_map_.begin();
         iter != window_visibility_map_.end(); ++iter) {
      iter->first->RemoveObserver(this);
      if (iter->second) {
        ScopedAnimationDisabler disabler(iter->first);
        iter->first->Show();
      }
    }

    DCHECK(!Shell::Get()->overview_controller()->InOverviewSession());

    // May reshow the home launcher after dragging.
    Shell::Get()->home_screen_controller()->OnWindowDragEnded();

    // Clears the background wallpaper blur.
    auto* wallpaper_view =
        RootWindowController::ForWindow(dragged_window_->GetRootWindow())
            ->wallpaper_widget_controller()
            ->wallpaper_view();
    if (wallpaper_view)
      wallpaper_view->RepaintBlurAndOpacity(kWallpaperClearBlurSigma, 1.f);
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window->RemoveObserver(this);
    window_visibility_map_.erase(window);
  }

  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (visible) {
      // Do not let |window| change to visible during the lifetime of |this|.
      // Also update |window_visibility_map_| so that we can restore the window
      // visibility correctly.
      window->Hide();
      window_visibility_map_[window] = visible;
    }
    // else do nothing. It must come from Hide() function above thus should be
    // ignored.
  }

 private:
  // The currently dragged window. Guaranteed to be non-nullptr during the
  // lifetime of |this|.
  aura::Window* dragged_window_;

  // Maintains the map between windows and their visibilities. All windows
  // except the dragged window and the source window should stay hidden during
  // dragging.
  std::map<aura::Window*, bool> window_visibility_map_;

  // The original backdrop mode of the source window. Should be disabled during
  // dragging.
  BackdropWindowMode source_window_backdrop_ = BackdropWindowMode::kAuto;

  DISALLOW_COPY_AND_ASSIGN(WindowsHider);
};

TabletModeBrowserWindowDragDelegate::TabletModeBrowserWindowDragDelegate() =
    default;

TabletModeBrowserWindowDragDelegate::~TabletModeBrowserWindowDragDelegate() =
    default;

void TabletModeBrowserWindowDragDelegate::PrepareWindowDrag(
    const gfx::Point& location_in_screen) {
  DCHECK(dragged_window_);

  WindowState* window_state = WindowState::Get(dragged_window_);
  window_state->OnDragStarted(window_state->drag_details()->window_component);
}

void TabletModeBrowserWindowDragDelegate::UpdateWindowDrag(
    const gfx::Point& location_in_screen) {
  DCHECK(dragged_window_);

  // Update the source window if necessary.
  UpdateSourceWindow(location_in_screen);
}

void TabletModeBrowserWindowDragDelegate::EndingWindowDrag(
    ToplevelWindowEventHandler::DragResult result,
    const gfx::Point& location_in_screen) {
  if (result == ToplevelWindowEventHandler::DragResult::SUCCESS)
    WindowState::Get(dragged_window_)->OnCompleteDrag(location_in_screen);
  else
    WindowState::Get(dragged_window_)->OnRevertDrag(location_in_screen);
}

void TabletModeBrowserWindowDragDelegate::EndedWindowDrag(
    const gfx::Point& location_in_screen) {
  MergeBackToSourceWindowIfApplicable(location_in_screen);
}

void TabletModeBrowserWindowDragDelegate::StartFling(
    const ui::GestureEvent* event) {
  if (event->details().velocity_y() > kFlingToStayAsNewWindowThreshold)
    can_merge_back_to_source_window_ = false;
}

bool TabletModeBrowserWindowDragDelegate::ShouldOpenOverviewWhenDragStarts() {
  DCHECK(dragged_window_);
  aura::Window* source_window =
      dragged_window_->GetProperty(ash::kTabDraggingSourceWindowKey);
  return !source_window;
}

void TabletModeBrowserWindowDragDelegate::UpdateSourceWindow(
    const gfx::Point& location_in_screen) {
  // Only do the scale if the source window is not the dragged window && the
  // source window is not in splitscreen && the source window is not in
  // overview.
  aura::Window* source_window =
      dragged_window_->GetProperty(ash::kTabDraggingSourceWindowKey);
  if (!source_window || source_window == dragged_window_ ||
      split_view_controller_->IsWindowInSplitView(source_window) ||
      source_window->GetProperty(ash::kIsShowingInOverviewKey)) {
    return;
  }

  // Only create WindowHider if we need to scale up/down the source window.
  if (!windows_hider_)
    windows_hider_ = std::make_unique<WindowsHider>(dragged_window_);

  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(dragged_window_)
          .work_area();
  gfx::Rect expected_bounds(work_area_bounds);
  if (location_in_screen.y() >=
      GetIndicatorsVerticalThreshold(work_area_bounds)) {
    SplitViewController::SnapPosition snap_position =
        GetSnapPosition(location_in_screen);

    if (snap_position == SplitViewController::NONE) {
      // Scale down the source window if the event location passes the vertical
      // |kIndicatorThresholdRatio| threshold.
      expected_bounds.ClampToCenteredSize(
          gfx::Size(work_area_bounds.width() * kSourceWindowScale,
                    work_area_bounds.height() * kSourceWindowScale));
    } else {
      // Put the source window on the other side of the split screen.
      expected_bounds = split_view_controller_->GetSnappedWindowBoundsInScreen(
          source_window, snap_position == SplitViewController::LEFT
                             ? SplitViewController::RIGHT
                             : SplitViewController::LEFT);
    }
  }
  ::wm::ConvertRectFromScreen(source_window->parent(), &expected_bounds);

  if (expected_bounds != source_window->GetTargetBounds()) {
    ui::ScopedLayerAnimationSettings settings(
        source_window->layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    source_window_bounds_observer_ =
        std::make_unique<SourceWindowAnimationObserver>(source_window,
                                                        dragged_window_);
    settings.AddObserver(source_window_bounds_observer_.get());
    source_window->SetBounds(expected_bounds);
  }
}

void TabletModeBrowserWindowDragDelegate::MergeBackToSourceWindowIfApplicable(
    const gfx::Point& location_in_screen) {
  // No need to merge back if we're not in tab dragging process.
  if (!window_util::IsDraggingTabs(dragged_window_))
    return;

  aura::Window* source_window =
      dragged_window_->GetProperty(ash::kTabDraggingSourceWindowKey);
  // Do not merge back if there is no source window.
  if (!source_window)
    return;

  // Do not merge back if the dragged window is not capable of merging back.
  // This may happen if the drag ends because of a fling event and the fling
  // velocity has exceeded kFlingToStayAsNewWindowThreshold.
  if (!can_merge_back_to_source_window_)
    return;

  // Do not merge back if the window has dragged farther than half of the screen
  // height.
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(dragged_window_)
          .work_area();
  if (location_in_screen.y() >= work_area_bounds.CenterPoint().y())
    return;

  SplitViewController::SnapPosition desired_snap_position =
      GetSnapPosition(location_in_screen);
  // If splitscreen is not active, do not merge back if the dragged window is
  // in the drag-to-snap preview area.
  if (!split_view_controller_->InSplitViewMode() &&
      desired_snap_position != SplitViewController::NONE) {
    return;
  }

  // In splitscreen, do not merge back if the drag point is on one side of the
  // split view divider and the source window is snapped on the opposite side.
  if (split_view_controller_->InSplitViewMode()) {
    const int drag_position = IsCurrentScreenOrientationLandscape()
                                  ? location_in_screen.x()
                                  : location_in_screen.y();
    const bool is_dragging_on_left =
        IsCurrentScreenOrientationPrimary()
            ? drag_position < split_view_controller_->divider_position()
            : drag_position > split_view_controller_->divider_position();
    aura::Window* window_on_opposite_side =
        is_dragging_on_left ? split_view_controller_->right_window()
                            : split_view_controller_->left_window();
    if (source_window == window_on_opposite_side)
      return;
  }

  // Arriving here we know the dragged window should merge back into its source
  // window.
  source_window->SetProperty(ash::kIsDeferredTabDraggingTargetWindowKey, true);
}

}  // namespace ash
