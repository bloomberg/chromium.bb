// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_animation_settings.h"

#include "ash/metrics/histogram_macros.h"
#include "ash/wm/overview/overview_constants.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"

namespace ash {

namespace {

// The time duration for fading out when closing an item.
constexpr base::TimeDelta kCloseFadeOut =
    base::TimeDelta::FromMilliseconds(100);

// The time duration for scaling down when an item is closed.
constexpr base::TimeDelta kCloseScale = base::TimeDelta::FromMilliseconds(100);

// The time duration for widgets to fade in.
constexpr base::TimeDelta kFadeInDelay = base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kFadeIn = base::TimeDelta::FromMilliseconds(167);

// The time duration for widgets to fade out.
constexpr base::TimeDelta kFadeOut = base::TimeDelta::FromMilliseconds(100);

constexpr base::TimeDelta kFromHomeLauncherDelay =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kHomeLauncherTransition =
    base::TimeDelta::FromMilliseconds(250);

// Time it takes for the selector widget to move to the next target. The same
// time is used for fading out shield widget when the overview mode is opened
// or closed.
constexpr base::TimeDelta kOverviewSelectorTransition =
    base::TimeDelta::FromMilliseconds(250);

// Time duration of the show animation of the drop target.
constexpr base::TimeDelta kDropTargetFadeIn =
    base::TimeDelta::FromMilliseconds(250);

base::TimeDelta GetAnimationDuration(OverviewAnimationType animation_type) {
  switch (animation_type) {
    case OVERVIEW_ANIMATION_NONE:
      return base::TimeDelta();
    case OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN:
      return kFadeIn;
    case OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT:
      return kFadeOut;
    case OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER:
    case OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW:
    case OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_EXIT:
    case OVERVIEW_ANIMATION_RESTORE_WINDOW:
    case OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO:
      return kTransition;
    case OVERVIEW_ANIMATION_CLOSING_OVERVIEW_ITEM:
      return kCloseScale;
    case OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM:
      return kCloseFadeOut;
    case OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER:
    case OVERVIEW_ANIMATION_EXIT_TO_HOME_LAUNCHER:
      return kHomeLauncherTransition;
    case OVERVIEW_ANIMATION_DROP_TARGET_FADE_IN:
      return kDropTargetFadeIn;
    case OVERVIEW_ANIMATION_NO_RECENTS_FADE:
    case OVERVIEW_ANIMATION_SELECTION_WINDOW_SHADOW:
    case OVERVIEW_ANIMATION_SELECTION_WINDOW:
      return kOverviewSelectorTransition;
  }
  NOTREACHED();
  return base::TimeDelta();
}

class OverviewEnterMetricsReporter : public ui::AnimationMetricsReporter {
 public:
  OverviewEnterMetricsReporter() = default;
  ~OverviewEnterMetricsReporter() override = default;

  void Report(int value) override {
    UMA_HISTOGRAM_PERCENTAGE("Ash.WindowSelector.AnimationSmoothness.Enter",
                             value);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverviewEnterMetricsReporter);
};

class OverviewExitMetricsReporter : public ui::AnimationMetricsReporter {
 public:
  OverviewExitMetricsReporter() = default;
  ~OverviewExitMetricsReporter() override = default;

  void Report(int value) override {
    UMA_HISTOGRAM_PERCENTAGE("Ash.WindowSelector.AnimationSmoothness.Exit",
                             value);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverviewExitMetricsReporter);
};

class OverviewCloseMetricsReporter : public ui::AnimationMetricsReporter {
 public:
  OverviewCloseMetricsReporter() = default;
  ~OverviewCloseMetricsReporter() override = default;

  void Report(int value) override {
    UMA_HISTOGRAM_PERCENTAGE("Ash.WindowSelector.AnimationSmoothness.Close",
                             value);
    UMA_HISTOGRAM_PERCENTAGE_IN_CLAMSHELL(
        "Ash.Overview.AnimationSmoothness.Close.ClamshellMode", value);
    UMA_HISTOGRAM_PERCENTAGE_IN_TABLET(
        "Ash.Overview.AnimationSmoothness.Close.TabletMode", value);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverviewCloseMetricsReporter);
};

base::LazyInstance<OverviewEnterMetricsReporter>::Leaky g_reporter_enter =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<OverviewExitMetricsReporter>::Leaky g_reporter_exit =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<OverviewCloseMetricsReporter>::Leaky g_reporter_close =
    LAZY_INSTANCE_INITIALIZER;

ui::AnimationMetricsReporter* GetMetricsReporter(
    OverviewAnimationType animation_type) {
  switch (animation_type) {
    case OVERVIEW_ANIMATION_NONE:
    case OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW:
    case OVERVIEW_ANIMATION_DROP_TARGET_FADE_IN:
    case OVERVIEW_ANIMATION_NO_RECENTS_FADE:
    case OVERVIEW_ANIMATION_SELECTION_WINDOW_SHADOW:
    case OVERVIEW_ANIMATION_SELECTION_WINDOW:
      return nullptr;
    case OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN:
    case OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER:
    case OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER:
      return g_reporter_enter.Pointer();
    case OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT:
    case OVERVIEW_ANIMATION_RESTORE_WINDOW:
    case OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO:
    case OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_EXIT:
    case OVERVIEW_ANIMATION_EXIT_TO_HOME_LAUNCHER:
      return g_reporter_exit.Pointer();
    case OVERVIEW_ANIMATION_CLOSING_OVERVIEW_ITEM:
    case OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM:
      return g_reporter_close.Pointer();
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

ScopedOverviewAnimationSettings::ScopedOverviewAnimationSettings(
    OverviewAnimationType animation_type,
    aura::Window* window)
    : ScopedOverviewAnimationSettings(animation_type,
                                      window->layer()->GetAnimator()) {}

ScopedOverviewAnimationSettings::ScopedOverviewAnimationSettings(
    OverviewAnimationType animation_type,
    ui::LayerAnimator* animator)
    : animation_settings_(
          std::make_unique<ui::ScopedLayerAnimationSettings>(animator)) {
  switch (animation_type) {
    case OVERVIEW_ANIMATION_NONE:
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
      break;
    case OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN:
      animation_settings_->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
      animator->SchedulePauseForProperties(kFadeInDelay,
                                           ui::LayerAnimationElement::OPACITY);
      break;
    case OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT:
      animation_settings_->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
      break;
    case OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER:
    case OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW:
    case OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_EXIT:
    case OVERVIEW_ANIMATION_RESTORE_WINDOW:
      animation_settings_->SetTweenType(gfx::Tween::EASE_OUT);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      break;
    case OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO:
      animation_settings_->SetTweenType(gfx::Tween::ZERO);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      break;
    case OVERVIEW_ANIMATION_CLOSING_OVERVIEW_ITEM:
    case OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM:
      animation_settings_->SetTweenType(gfx::Tween::EASE_OUT);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
      break;
    case OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER:
      animation_settings_->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
      animator->SchedulePauseForProperties(
          kFromHomeLauncherDelay, ui::LayerAnimationElement::OPACITY |
                                      ui::LayerAnimationElement::TRANSFORM);
      break;
    case OVERVIEW_ANIMATION_EXIT_TO_HOME_LAUNCHER:
      animation_settings_->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
      break;
    case OVERVIEW_ANIMATION_DROP_TARGET_FADE_IN:
      animation_settings_->SetTweenType(gfx::Tween::EASE_IN);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      break;
    case OVERVIEW_ANIMATION_NO_RECENTS_FADE:
    case OVERVIEW_ANIMATION_SELECTION_WINDOW_SHADOW:
      animation_settings_->SetTweenType(gfx::Tween::EASE_IN_OUT);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      break;
    case OVERVIEW_ANIMATION_SELECTION_WINDOW:
      animation_settings_->SetTweenType(gfx::Tween::EASE_OUT);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      break;
  }
  animation_settings_->SetTransitionDuration(
      GetAnimationDuration(animation_type));
  animation_settings_->SetAnimationMetricsReporter(
      GetMetricsReporter(animation_type));
}

ScopedOverviewAnimationSettings::~ScopedOverviewAnimationSettings() = default;

void ScopedOverviewAnimationSettings::AddObserver(
    ui::ImplicitAnimationObserver* observer) {
  animation_settings_->AddObserver(observer);
}

void ScopedOverviewAnimationSettings::CacheRenderSurface() {
  animation_settings_->CacheRenderSurface();
}

void ScopedOverviewAnimationSettings::DeferPaint() {
  animation_settings_->DeferPaint();
}

void ScopedOverviewAnimationSettings::TrilinearFiltering() {
  animation_settings_->TrilinearFiltering();
}

}  // namespace ash
