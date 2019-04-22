// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_utils.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The animation speed at which the highlights fade in or out.
constexpr base::TimeDelta kHighlightsFadeInOutMs =
    base::TimeDelta::FromMilliseconds(250);
// The animation speed which the other highlight fades in or out.
constexpr base::TimeDelta kOtherFadeInOutMs =
    base::TimeDelta::FromMilliseconds(133);
// The delay before the other highlight starts fading in.
constexpr base::TimeDelta kOtherFadeInDelayMs =
    base::TimeDelta::FromMilliseconds(117);
// The animation speed at which the preview area fades out (when you snap a
// window).
constexpr base::TimeDelta kPreviewAreaFadeOutMs =
    base::TimeDelta::FromMilliseconds(67);
// The time duration for the indicator label opacity animations.
constexpr base::TimeDelta kLabelAnimationMs =
    base::TimeDelta::FromMilliseconds(83);
// The delay before the indicator labels start fading in.
constexpr base::TimeDelta kLabelAnimationDelayMs =
    base::TimeDelta::FromMilliseconds(167);
// The time duration for the window transformation animations.
constexpr base::TimeDelta kWindowTransformMs =
    base::TimeDelta::FromMilliseconds(250);

constexpr float kHighlightOpacity = 0.3f;
constexpr float kPreviewAreaHighlightOpacity = 0.18f;

// Gets the duration, tween type and delay before animation based on |type|.
void GetAnimationValuesForType(
    SplitviewAnimationType type,
    base::TimeDelta* out_duration,
    gfx::Tween::Type* out_tween_type,
    ui::LayerAnimator::PreemptionStrategy* out_preemption_strategy,
    base::TimeDelta* out_delay) {
  *out_preemption_strategy = ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET;
  switch (type) {
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_IN:
    case SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_IN:
    case SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_OUT:
    case SPLITVIEW_ANIMATION_TEXT_FADE_IN_WITH_HIGHLIGHT:
    case SPLITVIEW_ANIMATION_TEXT_FADE_OUT_WITH_HIGHLIGHT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_OUT:
      *out_duration = kHighlightsFadeInOutMs;
      *out_tween_type = gfx::Tween::FAST_OUT_SLOW_IN;
      return;
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_IN:
      *out_delay = kOtherFadeInDelayMs;
      *out_duration = kOtherFadeInOutMs;
      *out_tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN;
      *out_preemption_strategy = ui::LayerAnimator::ENQUEUE_NEW_ANIMATION;
      return;
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_OUT:
      *out_duration = kOtherFadeInOutMs;
      *out_tween_type = gfx::Tween::FAST_OUT_LINEAR_IN;
      return;
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_NIX_INSET:
      *out_duration = kPreviewAreaFadeOutMs;
      *out_tween_type = gfx::Tween::FAST_OUT_LINEAR_IN;
      return;
    case SPLITVIEW_ANIMATION_TEXT_FADE_IN:
      *out_delay = kLabelAnimationDelayMs;
      *out_duration = kLabelAnimationMs;
      *out_tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN;
      *out_preemption_strategy = ui::LayerAnimator::ENQUEUE_NEW_ANIMATION;
      return;
    case SPLITVIEW_ANIMATION_TEXT_FADE_OUT:
      *out_duration = kLabelAnimationMs;
      *out_tween_type = gfx::Tween::FAST_OUT_LINEAR_IN;
      return;
    case SPLITVIEW_ANIMATION_SET_WINDOW_TRANSFORM:
      *out_duration = kWindowTransformMs;
      *out_tween_type = gfx::Tween::FAST_OUT_SLOW_IN;
      *out_preemption_strategy =
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET;
      return;
  }

  NOTREACHED();
}

// Helper function to apply animation values to |settings|.
void ApplyAnimationSettings(
    ui::ScopedLayerAnimationSettings* settings,
    ui::LayerAnimator* animator,
    ui::LayerAnimationElement::AnimatableProperties animated_property,
    base::TimeDelta duration,
    gfx::Tween::Type tween,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    base::TimeDelta delay) {
  DCHECK_EQ(settings->GetAnimator(), animator);
  settings->SetTransitionDuration(duration);
  settings->SetTweenType(tween);
  settings->SetPreemptionStrategy(preemption_strategy);
  if (!delay.is_zero())
    animator->SchedulePauseForProperties(delay, animated_property);
}

}  // namespace

void DoSplitviewOpacityAnimation(ui::Layer* layer,
                                 SplitviewAnimationType type) {
  float target_opacity = 0.f;
  switch (type) {
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_OUT:
    case SPLITVIEW_ANIMATION_TEXT_FADE_OUT:
    case SPLITVIEW_ANIMATION_TEXT_FADE_OUT_WITH_HIGHLIGHT:
      target_opacity = 0.f;
      break;
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_IN:
      target_opacity = kPreviewAreaHighlightOpacity;
      break;
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN:
      target_opacity = kHighlightOpacity;
      break;
    case SPLITVIEW_ANIMATION_OVERVIEW_ITEM_FADE_IN:
    case SPLITVIEW_ANIMATION_TEXT_FADE_IN:
    case SPLITVIEW_ANIMATION_TEXT_FADE_IN_WITH_HIGHLIGHT:
      target_opacity = 1.f;
      break;
    default:
      NOTREACHED() << "Not a valid split view opacity animation type.";
      return;
  }

  if (layer->GetTargetOpacity() == target_opacity)
    return;

  base::TimeDelta duration;
  gfx::Tween::Type tween;
  ui::LayerAnimator::PreemptionStrategy preemption_strategy;
  base::TimeDelta delay;
  GetAnimationValuesForType(type, &duration, &tween, &preemption_strategy,
                            &delay);

  ui::LayerAnimator* animator = layer->GetAnimator();
  ui::ScopedLayerAnimationSettings settings(animator);
  ApplyAnimationSettings(&settings, animator,
                         ui::LayerAnimationElement::OPACITY, duration, tween,
                         preemption_strategy, delay);
  layer->SetOpacity(target_opacity);
}

void DoSplitviewTransformAnimation(ui::Layer* layer,
                                   SplitviewAnimationType type,
                                   const gfx::Transform& target_transform) {
  if (layer->GetTargetTransform() == target_transform)
    return;

  switch (type) {
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_NIX_INSET:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_OUT:
    case SPLITVIEW_ANIMATION_SET_WINDOW_TRANSFORM:
      break;
    default:
      NOTREACHED() << "Not a valid split view transform type.";
      return;
  }

  base::TimeDelta duration;
  gfx::Tween::Type tween;
  ui::LayerAnimator::PreemptionStrategy preemption_strategy;
  base::TimeDelta delay;
  GetAnimationValuesForType(type, &duration, &tween, &preemption_strategy,
                            &delay);

  ui::LayerAnimator* animator = layer->GetAnimator();
  ui::ScopedLayerAnimationSettings settings(animator);
  ApplyAnimationSettings(&settings, animator,
                         ui::LayerAnimationElement::TRANSFORM, duration, tween,
                         preemption_strategy, delay);
  layer->SetTransform(target_transform);
}

bool IsClamshellSplitViewModeEnabled() {
  return base::FeatureList::IsEnabled(
      ash::features::kDragToSnapInClamshellMode);
}

bool ShouldAllowSplitView() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshDisableTabletSplitView)) {
    return false;
  }

  if (!Shell::Get()
           ->tablet_mode_controller()
           ->IsTabletModeWindowManagerEnabled() &&
      !IsClamshellSplitViewModeEnabled()) {
    return false;
  }

  // Don't allow split view if we're in pinned mode.
  if (Shell::Get()->screen_pinning_controller()->IsPinned())
    return false;

  // TODO(crubg.com/853588): Disallow window dragging and split screen while
  // ChromeVox is on until they are in a usable state.
  if (Shell::Get()->accessibility_controller()->spoken_feedback_enabled())
    return false;

  return true;
}

bool CanSnapInSplitview(aura::Window* window) {
  if (!::wm::CanActivateWindow(window))
    return false;

  if (!wm::GetWindowState(window)->CanSnap())
    return false;

  if (window->delegate()) {
    // If the window's minimum size is larger than half of the display's work
    // area size, the window can't be snapped in this case.
    const gfx::Size min_size = window->delegate()->GetMinimumSize();
    const gfx::Rect display_area =
        screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
            window);
    const bool is_landscape = (display_area.width() > display_area.height());
    if ((is_landscape && min_size.width() > display_area.width() / 2) ||
        (!is_landscape && min_size.height() > display_area.height() / 2)) {
      return false;
    }
  }

  return true;
}

}  // namespace ash
