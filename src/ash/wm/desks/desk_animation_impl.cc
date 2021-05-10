// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_animation_impl.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram_macros.h"

namespace ash {

namespace {

constexpr char kDeskActivationSmoothnessHistogramName[] =
    "Ash.Desks.AnimationSmoothness.DeskActivation";
constexpr char kDeskRemovalSmoothnessHistogramName[] =
    "Ash.Desks.AnimationSmoothness.DeskRemoval";

// Measures the presentation time during a continuous gesture animation. This is
// the time from when we receive an Update request to the time the next frame is
// presented.
constexpr char kDeskUpdateGestureHistogramName[] =
    "Ash.Desks.PresentationTime.UpdateGesture";
constexpr char kDeskUpdateGestureMaxLatencyHistogramName[] =
    "Ash.Desks.PresentationTime.UpdateGesture.MaxLatency";

// The user ends a gesture swipe and triggers an animation to the closest desk.
// This histogram measures the smoothness of that animation.
constexpr char kDeskEndGestureSmoothnessHistogramName[] =
    "Ash.Desks.AnimationSmoothness.DeskEndGesture";

// Swipes which are below this threshold are considered fast, and
// RootWindowDeskSwitchAnimator will determine a different ending desk for these
// swipes.
constexpr base::TimeDelta kFastSwipeThresholdDuration =
    base::TimeDelta::FromMilliseconds(500);

bool IsForContinuousGestures(DesksSwitchSource source) {
  return source == DesksSwitchSource::kDeskSwitchTouchpad &&
         features::IsEnhancedDeskAnimations();
}

}  // namespace

// -----------------------------------------------------------------------------
// DeskActivationAnimation:

DeskActivationAnimation::DeskActivationAnimation(DesksController* controller,
                                                 int starting_desk_index,
                                                 int ending_desk_index,
                                                 DesksSwitchSource source,
                                                 bool update_window_activation)
    : DeskAnimationBase(controller,
                        ending_desk_index,
                        IsForContinuousGestures(source)),
      switch_source_(source),
      update_window_activation_(update_window_activation),
      visible_desk_index_(starting_desk_index),
      last_start_or_replace_time_(base::TimeTicks::Now()),
      presentation_time_recorder_(CreatePresentationTimeHistogramRecorder(
          desks_util::GetSelectedCompositorForPerformanceMetrics(),
          kDeskUpdateGestureHistogramName,
          kDeskUpdateGestureMaxLatencyHistogramName)) {
  for (auto* root : Shell::GetAllRootWindows()) {
    desk_switch_animators_.emplace_back(
        std::make_unique<RootWindowDeskSwitchAnimator>(
            root, starting_desk_index, ending_desk_index, this,
            /*for_remove=*/false));
  }

  // On starting, the user may stay on the current desk for a touchpad swipe.
  // All other switch sources are guaranteed to move at least once.
  if (switch_source_ != DesksSwitchSource::kDeskSwitchTouchpad)
    visible_desk_changes_ = 1;
}

DeskActivationAnimation::~DeskActivationAnimation() = default;

bool DeskActivationAnimation::Replace(bool moving_left,
                                      DesksSwitchSource source) {
  // Replacing an animation of a different switch source is not supported.
  if (source != switch_source_)
    return false;

  // Do not log any EndSwipeAnimation smoothness metrics if the animation has
  // been canceled midway by an Replace call.
  if (is_continuous_gesture_animation_)
    throughput_tracker_.Cancel();

  // If any of the animators are still taking either screenshot, do not replace
  // the animation.
  for (const auto& animator : desk_switch_animators_) {
    if (!animator->starting_desk_screenshot_taken() ||
        !animator->ending_desk_screenshot_taken()) {
      return false;
    }
  }

  const int new_ending_desk_index = ending_desk_index_ + (moving_left ? -1 : 1);
  // Already at the leftmost or rightmost desk, nothing to replace.
  if (new_ending_desk_index < 0 ||
      new_ending_desk_index >= int{controller_->desks().size()}) {
    return false;
  }

  ending_desk_index_ = new_ending_desk_index;

  last_start_or_replace_time_ = base::TimeTicks::Now();

  // Similar to on starting, for touchpad, the user can replace the animation
  // without switching visible desks.
  if (switch_source_ != DesksSwitchSource::kDeskSwitchTouchpad)
    ++visible_desk_changes_;

  // List of animators that need a screenshot. It should be either empty or
  // match the size of |desk_switch_animators_| as all the animations should be
  // in sync.
  std::vector<RootWindowDeskSwitchAnimator*> pending_animators;
  for (const auto& animator : desk_switch_animators_) {
    if (animator->ReplaceAnimation(new_ending_desk_index))
      pending_animators.push_back(animator.get());
  }

  // No screenshot needed. Call OnEndingDeskScreenshotTaken which will start the
  // animation.
  if (pending_animators.empty()) {
    OnEndingDeskScreenshotTaken();
    return true;
  }

  // Activate the target desk and take a screenshot.
  // TODO(crbug.com/1134390): Convert back to DCHECK when the issue is fixed.
  CHECK_EQ(pending_animators.size(), desk_switch_animators_.size());
  PrepareDeskForScreenshot(new_ending_desk_index);
  for (auto* animator : pending_animators)
    animator->TakeEndingDeskScreenshot();
  return true;
}

bool DeskActivationAnimation::UpdateSwipeAnimation(float scroll_delta_x) {
  if (!is_continuous_gesture_animation_)
    return false;

  presentation_time_recorder_->RequestNext();

  // If any of the displays need a new screenshot while scrolling, take the
  // ending desk screenshot for all of them to keep them in sync.
  base::Optional<int> ending_desk_index;
  for (const auto& animator : desk_switch_animators_) {
    if (!ending_desk_index)
      ending_desk_index = animator->UpdateSwipeAnimation(scroll_delta_x);
    else
      animator->UpdateSwipeAnimation(scroll_delta_x);
  }

  // See if the animator of the first display has visibly changed desks. If so,
  // update |visible_desk_changes_| for metrics collection purposes.
  auto* first_animator = desk_switch_animators_.front().get();
  DCHECK(first_animator);
  if (first_animator->starting_desk_screenshot_taken() &&
      first_animator->ending_desk_screenshot_taken()) {
    const int old_visible_desk_index = visible_desk_index_;
    visible_desk_index_ = first_animator->GetIndexOfMostVisibleDeskScreenshot();
    if (visible_desk_index_ != old_visible_desk_index)
      ++visible_desk_changes_;
  }

  // No screenshot needed.
  if (!ending_desk_index)
    return true;

  // Activate the target desk and take a screenshot.
  ending_desk_index_ = *ending_desk_index;
  PrepareDeskForScreenshot(ending_desk_index_);
  for (const auto& animator : desk_switch_animators_) {
    animator->PrepareForEndingDeskScreenshot(ending_desk_index_);
    animator->TakeEndingDeskScreenshot();
  }
  return true;
}

bool DeskActivationAnimation::EndSwipeAnimation() {
  if (!is_continuous_gesture_animation_)
    return false;

  // Start tracking the animation smoothness after the continuous gesture swipe
  // has ended.
  throughput_tracker_.Start(
      metrics_util::ForSmoothness(base::BindRepeating([](int smoothness) {
        UMA_HISTOGRAM_PERCENTAGE(kDeskEndGestureSmoothnessHistogramName,
                                 smoothness);
      })));

  // End the animation. The animator will determine which desk to animate to,
  // and update their ending desk index. When the animation is finished we will
  // activate that desk.
  const bool is_fast_swipe =
      base::TimeTicks::Now() - last_start_or_replace_time_ <
      kFastSwipeThresholdDuration;
  for (const auto& animator : desk_switch_animators_)
    ending_desk_index_ = animator->EndSwipeAnimation(is_fast_swipe);

  return true;
}

void DeskActivationAnimation::OnStartingDeskScreenshotTakenInternal(
    int ending_desk_index) {
  DCHECK_EQ(ending_desk_index_, ending_desk_index);
  PrepareDeskForScreenshot(ending_desk_index);
}

void DeskActivationAnimation::OnDeskSwitchAnimationFinishedInternal() {
  // During a chained animation we may not switch desks if a replaced target
  // desk does not require a new screenshot. If that is the case, activate the
  // proper desk here.
  controller_->ActivateDeskInternal(
      controller_->desks()[ending_desk_index_].get(),
      update_window_activation_);
}

metrics_util::ReportCallback DeskActivationAnimation::GetReportCallback()
    const {
  return metrics_util::ForSmoothness(base::BindRepeating([](int smoothness) {
    UMA_HISTOGRAM_PERCENTAGE(kDeskActivationSmoothnessHistogramName,
                             smoothness);
  }));
}

void DeskActivationAnimation::PrepareDeskForScreenshot(int index) {
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers())
    root_window_controller->HideContextMenuNoAnimation();

  // The order here matters. Overview must end before ending tablet split view
  // before switching desks. (If clamshell split view is active on one or more
  // displays, then it simply will end when we end overview.) That's because
  // we don't want |TabletModeWindowManager| maximizing all windows because we
  // cleared the snapped ones in |SplitViewController| first. See
  // |TabletModeWindowManager::OnOverviewModeEndingAnimationComplete|.
  // See also test coverage for this case in
  // `TabletModeDesksTest.SnappedStateRetainedOnSwitchingDesksFromOverview`.
  const bool in_overview =
      Shell::Get()->overview_controller()->InOverviewSession();
  if (in_overview) {
    // Exit overview mode immediately without any animations before taking the
    // ending desk screenshot. This makes sure that the ending desk
    // screenshot will only show the windows in that desk, not overview stuff.
    Shell::Get()->overview_controller()->EndOverview(
        OverviewEnterExitType::kImmediateExit);
  }
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->EndSplitView(
      SplitViewController::EndReason::kDesksChange);

  controller_->ActivateDeskInternal(
      controller_->desks()[ending_desk_index_].get(),
      update_window_activation_);

  MaybeRestoreSplitView(/*refresh_snapped_windows=*/true);
}

// -----------------------------------------------------------------------------
// DeskRemovalAnimation:

DeskRemovalAnimation::DeskRemovalAnimation(DesksController* controller,
                                           int desk_to_remove_index,
                                           int desk_to_activate_index,
                                           DesksCreationRemovalSource source)
    : DeskAnimationBase(controller,
                        desk_to_activate_index,
                        /*is_continuous_gesture_animation=*/false),
      desk_to_remove_index_(desk_to_remove_index),
      request_source_(source) {
  DCHECK(!Shell::Get()->overview_controller()->InOverviewSession());
  DCHECK_EQ(controller_->active_desk(),
            controller_->desks()[desk_to_remove_index_].get());

  for (auto* root : Shell::GetAllRootWindows()) {
    desk_switch_animators_.emplace_back(
        std::make_unique<RootWindowDeskSwitchAnimator>(
            root, desk_to_remove_index_, desk_to_activate_index, this,
            /*for_remove=*/true));
  }
}

DeskRemovalAnimation::~DeskRemovalAnimation() = default;

void DeskRemovalAnimation::OnStartingDeskScreenshotTakenInternal(
    int ending_desk_index) {
  DCHECK_EQ(ending_desk_index_, ending_desk_index);
  DCHECK_EQ(controller_->active_desk(),
            controller_->desks()[desk_to_remove_index_].get());

  // We are removing the active desk, which may have tablet split view active.
  // We will restore the split view state of the newly activated desk at the
  // end of the animation. Clamshell split view is impossible because
  // |DeskRemovalAnimation| is not used in overview.
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->EndSplitView(
      SplitViewController::EndReason::kDesksChange);

  for (auto* root_window_controller : Shell::GetAllRootWindowControllers())
    root_window_controller->HideContextMenuNoAnimation();

  // At the end of phase (1), we activate the target desk (i.e. the desk that
  // will be activated after the active desk `desk_to_remove_index_` is
  // removed). This means that phase (2) will take a screenshot of that desk
  // before we move the windows of `desk_to_remove_index_` to that target desk.
  controller_->ActivateDeskInternal(
      controller_->desks()[ending_desk_index_].get(),
      /*update_window_activation=*/false);
}

void DeskRemovalAnimation::OnDeskSwitchAnimationFinishedInternal() {
  // Do the actual desk removal behind the scenes before the screenshot layers
  // are destroyed.
  controller_->RemoveDeskInternal(
      controller_->desks()[desk_to_remove_index_].get(), request_source_);

  MaybeRestoreSplitView(/*refresh_snapped_windows=*/true);
}

metrics_util::ReportCallback DeskRemovalAnimation::GetReportCallback() const {
  return ash::metrics_util::ForSmoothness(
      base::BindRepeating([](int smoothness) {
        UMA_HISTOGRAM_PERCENTAGE(kDeskRemovalSmoothnessHistogramName,
                                 smoothness);
      }));
}

}  // namespace ash
