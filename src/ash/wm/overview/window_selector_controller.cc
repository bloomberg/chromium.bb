// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_controller.h"

#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Do not blur or unblur the wallpaper when entering or exiting overview mode
// when this is true.
bool g_disable_wallpaper_blur_for_tests = false;

constexpr int kBlurSlideDurationMs = 250;

// It can take up to two frames until the frame created in the UI thread that
// triggered animation observer is drawn. Wait 50ms in attemp to
// let its draw and swap finish.
constexpr int kOcclusionPauseDurationForStartMs = 50;

// Wait longer when exiting overview mode in case when a user
// may re-enter overview mode immediately, contents are ready.
constexpr int kOcclusionPauseDurationForEndMs = 500;

bool IsBlurAllowed() {
  return !g_disable_wallpaper_blur_for_tests &&
         Shell::Get()->wallpaper_controller()->IsBlurAllowed();
}

// Returns whether overview mode items should be slid in or out from the top of
// the screen.
bool ShouldSlideInOutOverview(const std::vector<aura::Window*>& windows) {
  // No sliding if home launcher is not available.
  if (!Shell::Get()
           ->tablet_mode_controller()
           ->IsTabletModeWindowManagerEnabled()) {
    return false;
  }

  if (windows.empty())
    return false;

  // Only slide in if all windows are minimized.
  for (const aura::Window* window : windows) {
    if (!wm::GetWindowState(window)->IsMinimized())
      return false;
  }

  return true;
}

}  // namespace

// Class that handles of blurring wallpaper upon entering and exiting overview
// mode. Blurs the wallpaper automatically if the wallpaper is not visible
// prior to entering overview mode (covered by a window), otherwise animates
// the blur.
class WindowSelectorController::OverviewBlurController
    : public ui::CompositorAnimationObserver,
      public aura::WindowObserver {
 public:
  OverviewBlurController() = default;

  ~OverviewBlurController() override {
    if (compositor_)
      compositor_->RemoveAnimationObserver(this);
    for (aura::Window* root : roots_to_animate_)
      root->RemoveObserver(this);
  }

  void Blur() { OnBlurChange(WallpaperAnimationState::kAddingBlur); }

  void Unblur() { OnBlurChange(WallpaperAnimationState::kRemovingBlur); }

 private:
  enum class WallpaperAnimationState {
    kAddingBlur,
    kRemovingBlur,
    kNormal,
  };

  void OnAnimationStep(base::TimeTicks timestamp) override {
    if (start_time_ == base::TimeTicks()) {
      start_time_ = timestamp;
      return;
    }
    const float progress = (timestamp - start_time_).InMilliseconds() /
                           static_cast<float>(kBlurSlideDurationMs);
    const bool adding = state_ == WallpaperAnimationState::kAddingBlur;
    if (progress > 1.0f) {
      AnimationProgressed(adding ? 1.0f : 0.f);
      Stop();
    } else {
      AnimationProgressed(adding ? progress : 1.f - progress);
    }
  }

  void OnCompositingShuttingDown(ui::Compositor* compositor) override {
    if (compositor_ == compositor)
      Stop();
  }

  void Stop() {
    if (compositor_) {
      compositor_->RemoveAnimationObserver(this);
      compositor_ = nullptr;
    }
    state_ = WallpaperAnimationState::kNormal;
  }

  void Start() {
    DCHECK(!compositor_);
    compositor_ = Shell::GetPrimaryRootWindow()->GetHost()->compositor();
    compositor_->AddAnimationObserver(this);
    start_time_ = base::TimeTicks();
  }

  void AnimationProgressed(float value) {
    // Animate only to even numbers to reduce the load.
    int ivalue = static_cast<int>(value * kWallpaperBlurSigma) / 2 * 2;
    for (aura::Window* root : roots_to_animate_)
      ApplyBlur(root, ivalue);
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window->RemoveObserver(this);
    auto it =
        std::find(roots_to_animate_.begin(), roots_to_animate_.end(), window);
    if (it != roots_to_animate_.end())
      roots_to_animate_.erase(it);
  }

  void ApplyBlur(aura::Window* root, float blur_sigma) {
    RootWindowController::ForWindow(root)
        ->wallpaper_widget_controller()
        ->SetWallpaperBlur(blur_sigma);
  }

  // Called when the wallpaper is to be changed. Checks to see which root
  // windows should have their wallpaper blurs animated and fills
  // |roots_to_animate_| accordingly. Applys blur or unblur immediately if
  // the wallpaper does not need blur animation.
  void OnBlurChange(WallpaperAnimationState state) {
    Stop();
    for (aura::Window* root : roots_to_animate_)
      root->RemoveObserver(this);
    roots_to_animate_.clear();

    state_ = state;
    const bool should_blur = state_ == WallpaperAnimationState::kAddingBlur;
    const float value =
        should_blur ? kWallpaperBlurSigma : kWallpaperClearBlurSigma;

    WindowSelector* window_selector =
        Shell::Get()->window_selector_controller()->window_selector();
    for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
      // No need to animate the blur on exiting as this should only be called
      // after overview animations are finished.
      if (should_blur) {
        DCHECK(window_selector);
        if (window_selector->ShouldAnimateWallpaper(root)) {
          root->AddObserver(this);
          roots_to_animate_.push_back(root);
          continue;
        }
      }
      ApplyBlur(root, value);
    }

    // Run the animation if one of the roots needs to be animated.
    if (roots_to_animate_.empty())
      state_ = WallpaperAnimationState::kNormal;
    else
      Start();
  }

  ui::Compositor* compositor_ = nullptr;
  base::TimeTicks start_time_;

  WallpaperAnimationState state_ = WallpaperAnimationState::kNormal;
  // Vector which contains the root windows, if any, whose wallpaper should have
  // blur animated after Blur or Unblur is called.
  std::vector<aura::Window*> roots_to_animate_;

  DISALLOW_COPY_AND_ASSIGN(OverviewBlurController);
};

WindowSelectorController::WindowSelectorController()
    : occlusion_pause_duration_for_end_ms_(kOcclusionPauseDurationForEndMs),
      overview_blur_controller_(std::make_unique<OverviewBlurController>()),
      weak_ptr_factory_(this) {
  Shell::Get()->activation_client()->AddObserver(this);
}

WindowSelectorController::~WindowSelectorController() {
  Shell::Get()->activation_client()->RemoveObserver(this);
  overview_blur_controller_.reset();

  // Destroy widgets that may be still animating if shell shuts down soon after
  // exiting overview mode.
  for (std::unique_ptr<DelayedAnimationObserver>& animation_observer :
       delayed_animations_) {
    animation_observer->Shutdown();
  }

  if (window_selector_.get()) {
    window_selector_->Shutdown();
    window_selector_.reset();
  }
}

// static
bool WindowSelectorController::CanSelect() {
  // Don't allow a window overview if the user session is not active (e.g.
  // locked or in user-adding screen) or a modal dialog is open or running in
  // kiosk app session.
  SessionController* session_controller = Shell::Get()->session_controller();
  return session_controller->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         !Shell::IsSystemModalWindowOpen() &&
         !Shell::Get()->screen_pinning_controller()->IsPinned() &&
         !session_controller->IsRunningInAppMode();
}

bool WindowSelectorController::ToggleOverview(
    WindowSelector::EnterExitOverviewType type) {
  // Hide the virtual keyboard as it obstructs the overview mode.
  // Don't need to hide if it's the a11y keyboard, as overview mode
  // can accept text input and it resizes correctly with the a11y keyboard.
  keyboard::KeyboardController::Get()->HideKeyboardImplicitlyByUser();

  auto windows = Shell::Get()->mru_window_tracker()->BuildMruWindowList();

  // Hidden windows will be removed by wm::ShouldExcludeForOverview so we
  // must copy them out first.
  std::vector<aura::Window*> hide_windows(windows.size());
  auto end = std::copy_if(
      windows.begin(), windows.end(), hide_windows.begin(),
      [](aura::Window* w) { return w->GetProperty(kHideInOverviewKey); });
  hide_windows.resize(end - hide_windows.begin());
  base::EraseIf(windows, wm::ShouldExcludeForOverview);

  // We may want to slide the overview grid in or out in some cases, even if
  // not explicitly stated.
  WindowSelector::EnterExitOverviewType new_type = type;
  if (type == WindowSelector::EnterExitOverviewType::kNormal &&
      ShouldSlideInOutOverview(windows)) {
    new_type = WindowSelector::EnterExitOverviewType::kWindowsMinimized;
  }

  if (IsSelecting()) {
    // Do not allow ending overview if we're in single split mode unless swiping
    // up from the shelf.
    if (windows.empty() && Shell::Get()->IsSplitViewModeActive() &&
        type != WindowSelector::EnterExitOverviewType::kSwipeFromShelf) {
      return true;
    }

    // Suspend occlusion tracker until the exit animation is complete.
    reset_pauser_task_.Cancel();
    occlusion_tracker_pauser_ =
        std::make_unique<aura::WindowOcclusionTracker::ScopedPause>(
            Shell::Get()->aura_env());

    window_selector_->set_enter_exit_overview_type(new_type);
    if (type == WindowSelector::EnterExitOverviewType::kWindowsMinimized ||
        type == WindowSelector::EnterExitOverviewType::kSwipeFromShelf) {
      // Minimize the windows without animations. When the home launcher button
      // is pressed, minimized widgets will get created in their place, and
      // those widgets will be slid out of overview. Otherwise,
      // HomeLauncherGestureHandler will handle sliding the windows out and when
      // this function is called, we do not need to create minimized widgets.
      for (aura::Window* window : windows) {
        if (wm::GetWindowState(window)->IsMinimized())
          continue;

        ScopedAnimationDisabler disable(window);
        window->Hide();
        wm::GetWindowState(window)->Minimize();
      }
    }

    OnSelectionEnded();
  } else {
    // Don't start overview if window selection is not allowed.
    if (!CanSelect())
      return false;

    // Clear any animations that may be running from last overview end.
    for (const auto& animation : delayed_animations_)
      animation->Shutdown();
    if (!delayed_animations_.empty())
      OnEndingAnimationComplete(/*canceled=*/true);
    delayed_animations_.clear();

    // Suspend occlusion tracker until the enter animation is complete.
    reset_pauser_task_.Cancel();
    occlusion_tracker_pauser_ =
        std::make_unique<aura::WindowOcclusionTracker::ScopedPause>(
            Shell::Get()->aura_env());

    window_selector_ = std::make_unique<WindowSelector>(this);
    window_selector_->set_enter_exit_overview_type(new_type);
    Shell::Get()->NotifyOverviewModeStarting();
    window_selector_->Init(windows, hide_windows);
    if (IsBlurAllowed())
      overview_blur_controller_->Blur();
    if (start_animations_.empty())
      OnStartingAnimationComplete(/*canceled=*/false);
    OnSelectionStarted();
  }
  return true;
}

void WindowSelectorController::OnStartingAnimationComplete(bool canceled) {
  Shell::Get()->NotifyOverviewModeStartingAnimationComplete(canceled);
  if (window_selector_)
    window_selector_->OnStartingAnimationComplete(canceled);
  reset_pauser_task_.Reset(base::BindOnce(
      &WindowSelectorController::ResetPauser, weak_ptr_factory_.GetWeakPtr()));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, reset_pauser_task_.callback(),
      base::TimeDelta::FromMilliseconds(kOcclusionPauseDurationForStartMs));
}

void WindowSelectorController::OnEndingAnimationComplete(bool canceled) {
  Shell::Get()->NotifyOverviewModeEndingAnimationComplete(canceled);
  reset_pauser_task_.Reset(base::BindOnce(
      &WindowSelectorController::ResetPauser, weak_ptr_factory_.GetWeakPtr()));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, reset_pauser_task_.callback(),
      base::TimeDelta::FromMilliseconds(occlusion_pause_duration_for_end_ms_));
}

void WindowSelectorController::ResetPauser() {
  occlusion_tracker_pauser_.reset();
}

bool WindowSelectorController::IsSelecting() const {
  return window_selector_ != nullptr;
}

bool WindowSelectorController::IsCompletingShutdownAnimations() {
  return !delayed_animations_.empty();
}

void WindowSelectorController::IncrementSelection(int increment) {
  DCHECK(IsSelecting());
  window_selector_->IncrementSelection(increment);
}

bool WindowSelectorController::AcceptSelection() {
  DCHECK(IsSelecting());
  return window_selector_->AcceptSelection();
}

void WindowSelectorController::OnOverviewButtonTrayLongPressed(
    const gfx::Point& event_location) {
  // Do nothing if split view is not enabled.
  if (!SplitViewController::ShouldAllowSplitView())
    return;

  // Depending on the state of the windows and split view, a long press has many
  // different results.
  // 1. Already in split view - exit split view. Activate the left window if it
  // is snapped left or both sides. Activate the right window if it is snapped
  // right.
  // 2. Not in overview mode - enter split view iff
  //     a) there is an active window
  //     b) there are at least two windows in the mru list
  //     c) the active window is snappable
  // 3. In overview mode - enter split view iff
  //     a) there are at least two windows in the mru list
  //     b) the first window in the mru list is snappable

  auto* split_view_controller = Shell::Get()->split_view_controller();
  // Exit split view mode if we are already in it.
  if (split_view_controller->IsSplitViewModeActive()) {
    // In some cases the window returned by wm::GetActiveWindow will be an item
    // in overview mode (maybe the overview mode text selection widget). The
    // active window may also be a transient descendant of the left or right
    // snapped window, in which we want to activate the transient window's
    // ancestor (left or right snapped window). Manually set |active_window| as
    // either the left or right window.
    aura::Window* active_window = wm::GetActiveWindow();
    while (::wm::GetTransientParent(active_window))
      active_window = ::wm::GetTransientParent(active_window);
    if (active_window != split_view_controller->left_window() &&
        active_window != split_view_controller->right_window()) {
      active_window = split_view_controller->GetDefaultSnappedWindow();
    }
    DCHECK(active_window);
    split_view_controller->EndSplitView();
    if (IsSelecting())
      ToggleOverview();
    ::wm::ActivateWindow(active_window);
    base::RecordAction(
        base::UserMetricsAction("Tablet_LongPressOverviewButtonExitSplitView"));
    return;
  }

  WindowSelectorItem* item_to_snap = nullptr;
  if (!IsSelecting()) {
    // The current active window may be a transient child.
    aura::Window* active_window = wm::GetActiveWindow();
    while (active_window && ::wm::GetTransientParent(active_window))
      active_window = ::wm::GetTransientParent(active_window);

    // Do nothing if there are no active windows or less than two windows to
    // work with.
    if (!active_window ||
        Shell::Get()->mru_window_tracker()->BuildWindowForCycleList().size() <
            2u) {
      return;
    }

    // Show a toast if the window cannot be snapped.
    if (!split_view_controller->CanSnap(active_window)) {
      split_view_controller->ShowAppCannotSnapToast();
      return;
    }

    // If we are not in overview mode, enter overview mode and then find the
    // window item to snap.
    ToggleOverview();
    DCHECK(window_selector_);
    WindowGrid* current_grid =
        window_selector_->GetGridWithRootWindow(active_window->GetRootWindow());
    if (current_grid) {
      item_to_snap =
          current_grid->GetWindowSelectorItemContaining(active_window);
    }
  } else {
    // Currently in overview mode, with no snapped windows. Retrieve the first
    // window selector item and attempt to snap that window.
    DCHECK(window_selector_);
    WindowGrid* current_grid = window_selector_->GetGridWithRootWindow(
        wm::GetRootWindowAt(event_location));
    if (current_grid) {
      const auto& windows = current_grid->window_list();
      if (windows.size() > 1)
        item_to_snap = windows[0].get();
    }
  }

  // Do nothing if no item was retrieved, or if the retrieved item is
  // unsnappable.
  if (!item_to_snap ||
      !split_view_controller->CanSnap(item_to_snap->GetWindow())) {
    return;
  }

  split_view_controller->SnapWindow(item_to_snap->GetWindow(),
                                    SplitViewController::LEFT);
  base::RecordAction(
      base::UserMetricsAction("Tablet_LongPressOverviewButtonEnterSplitView"));
}

std::vector<aura::Window*>
WindowSelectorController::GetWindowsListInOverviewGridsForTesting() {
  std::vector<aura::Window*> windows;
  for (const std::unique_ptr<WindowGrid>& grid :
       window_selector_->grid_list_for_testing()) {
    for (const auto& window_selector_item : grid->window_list())
      windows.push_back(window_selector_item->GetWindow());
  }
  return windows;
}

// TODO(flackr): Make WindowSelectorController observe the activation of
// windows, so we can remove WindowSelectorDelegate.
void WindowSelectorController::OnSelectionEnded() {
  if (is_shutting_down_)
    return;

  if (!occlusion_tracker_pauser_) {
    reset_pauser_task_.Cancel();
    occlusion_tracker_pauser_ =
        std::make_unique<aura::WindowOcclusionTracker::ScopedPause>(
            Shell::Get()->aura_env());
  }

  if (!start_animations_.empty())
    OnStartingAnimationComplete(/*canceled=*/true);
  start_animations_.clear();

  window_selector_->UpdateMaskAndShadow(/*show=*/false);
  is_shutting_down_ = true;
  Shell::Get()->NotifyOverviewModeEnding();
  auto* window_selector = window_selector_.release();
  window_selector->Shutdown();
  // Don't delete |window_selector_| yet since the stack is still using it.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, window_selector);
  last_selection_time_ = base::Time::Now();
  Shell::Get()->NotifyOverviewModeEnded();
  is_shutting_down_ = false;
  // There may be no delayed animations in tests, so unblur right away.
  if (delayed_animations_.empty()) {
    if (IsBlurAllowed())
      overview_blur_controller_->Unblur();
    OnEndingAnimationComplete(/*canceled=*/false);
  }
}

void WindowSelectorController::AddDelayedAnimationObserver(
    std::unique_ptr<DelayedAnimationObserver> animation_observer) {
  animation_observer->SetOwner(this);
  delayed_animations_.push_back(std::move(animation_observer));
}

void WindowSelectorController::RemoveAndDestroyAnimationObserver(
    DelayedAnimationObserver* animation_observer) {
  const bool previous_empty = delayed_animations_.empty();
  base::EraseIf(delayed_animations_,
                base::MatchesUniquePtr(animation_observer));

  // If something has been removed and its the last observer, unblur the
  // wallpaper and let observers know. This function may be called while still
  // in overview (ie. splitview restores one window but leaves overview active)
  // so check that |window_selector_| is null before notifying.
  if (!window_selector_ && !previous_empty && delayed_animations_.empty()) {
    if (IsBlurAllowed())
      overview_blur_controller_->Unblur();

    OnEndingAnimationComplete(/*canceled=*/false);
  }
}

void WindowSelectorController::OnWindowActivating(ActivationReason reason,
                                                  aura::Window* gained_active,
                                                  aura::Window* lost_active) {
  if (window_selector_)
    window_selector_->OnWindowActivating(reason, gained_active, lost_active);
}

void WindowSelectorController::OnAttemptToReactivateWindow(
    aura::Window* request_active,
    aura::Window* actual_active) {
  if (window_selector_) {
    window_selector_->OnWindowActivating(
        ::wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
        request_active, actual_active);
  }
}

void WindowSelectorController::AddStartAnimationObserver(
    std::unique_ptr<DelayedAnimationObserver> animation_observer) {
  animation_observer->SetOwner(this);
  start_animations_.push_back(std::move(animation_observer));
}

void WindowSelectorController::RemoveAndDestroyStartAnimationObserver(
    DelayedAnimationObserver* animation_observer) {
  const bool previous_empty = start_animations_.empty();
  base::EraseIf(start_animations_, base::MatchesUniquePtr(animation_observer));

  if (!previous_empty && start_animations_.empty())
    OnStartingAnimationComplete(/*canceled=*/false);
}

// static
void WindowSelectorController::SetDoNotChangeWallpaperBlurForTests() {
  g_disable_wallpaper_blur_for_tests = true;
}

void WindowSelectorController::OnSelectionStarted() {
  if (!last_selection_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Ash.WindowSelector.TimeBetweenUse",
                             base::Time::Now() - last_selection_time_);
  }
}

}  // namespace ash
