// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation_controller_impl.h"

#include "base/auto_reset.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_hover.h"
#include "ui/views/animation/square_ink_drop_animation.h"

namespace views {

namespace {

// The duration, in milliseconds, of the hover state fade in animation when it
// is triggered by user input.
const int kHoverFadeInFromUserInputDurationInMs = 250;

// The duration, in milliseconds, of the hover state fade out animation when it
// is triggered by user input.
const int kHoverFadeOutFromUserInputDurationInMs = 250;

// The duration, in milliseconds, of the hover state fade in animation when it
// is triggered by an ink drop ripple animation ending.
const int kHoverFadeInAfterAnimationDurationInMs = 250;

// The duration, in milliseconds, of the hover state fade out animation when it
// is triggered by an ink drop ripple animation starting.
const int kHoverFadeOutBeforeAnimationDurationInMs = 120;

// The amount of time in milliseconds that |hover_| should delay after a ripple
// animation before fading in.
const int kHoverFadeInAfterAnimationDelayInMs = 1000;

// Returns true if an ink drop with the given |ink_drop_state| should
// automatically transition to the InkDropState::HIDDEN state.
bool ShouldAnimateToHidden(InkDropState ink_drop_state) {
  switch (ink_drop_state) {
    case views::InkDropState::ACTION_TRIGGERED:
    case views::InkDropState::ALTERNATE_ACTION_TRIGGERED:
    case views::InkDropState::DEACTIVATED:
      return true;
    default:
      return false;
  }
}

}  // namespace

InkDropAnimationControllerImpl::InkDropAnimationControllerImpl(
    InkDropHost* ink_drop_host)
    : ink_drop_host_(ink_drop_host),
      root_layer_(new ui::Layer(ui::LAYER_NOT_DRAWN)),
      is_hovered_(false),
      hover_after_animation_timer_(nullptr) {
  root_layer_->set_name("InkDropAnimationControllerImpl:RootLayer");
  ink_drop_host_->AddInkDropLayer(root_layer_.get());
}

InkDropAnimationControllerImpl::~InkDropAnimationControllerImpl() {
  // Explicitly destroy the InkDropAnimation so that this still exists if
  // views::InkDropAnimationObserver methods are called on this.
  DestroyInkDropAnimation();
  ink_drop_host_->RemoveInkDropLayer(root_layer_.get());
}

InkDropState InkDropAnimationControllerImpl::GetTargetInkDropState() const {
  if (!ink_drop_animation_)
    return InkDropState::HIDDEN;
  return ink_drop_animation_->target_ink_drop_state();
}

bool InkDropAnimationControllerImpl::IsVisible() const {
  return ink_drop_animation_ && ink_drop_animation_->IsVisible();
}

void InkDropAnimationControllerImpl::AnimateToState(
    InkDropState ink_drop_state) {
  DestroyHiddenTargetedAnimations();
  if (!ink_drop_animation_)
    CreateInkDropAnimation();

  if (ink_drop_state != views::InkDropState::HIDDEN) {
    SetHoveredInternal(false, base::TimeDelta::FromMilliseconds(
                                  kHoverFadeOutBeforeAnimationDurationInMs),
                       true);
  }

  ink_drop_animation_->AnimateToState(ink_drop_state);
}

void InkDropAnimationControllerImpl::SnapToActivated() {
  DestroyHiddenTargetedAnimations();
  if (!ink_drop_animation_)
    CreateInkDropAnimation();

  SetHoveredInternal(false, base::TimeDelta(), false);

  ink_drop_animation_->SnapToActivated();
}

void InkDropAnimationControllerImpl::SetHovered(bool is_hovered) {
  is_hovered_ = is_hovered;
  SetHoveredInternal(is_hovered,
                     is_hovered ? base::TimeDelta::FromMilliseconds(
                                      kHoverFadeInFromUserInputDurationInMs)
                                : base::TimeDelta::FromMilliseconds(
                                      kHoverFadeOutFromUserInputDurationInMs),
                     false);
}

void InkDropAnimationControllerImpl::DestroyHiddenTargetedAnimations() {
  if (ink_drop_animation_ &&
      (ink_drop_animation_->target_ink_drop_state() == InkDropState::HIDDEN ||
       ShouldAnimateToHidden(ink_drop_animation_->target_ink_drop_state()))) {
    DestroyInkDropAnimation();
  }
}

void InkDropAnimationControllerImpl::CreateInkDropAnimation() {
  DestroyInkDropAnimation();
  ink_drop_animation_ = ink_drop_host_->CreateInkDropAnimation();
  ink_drop_animation_->set_observer(this);
  root_layer_->Add(ink_drop_animation_->GetRootLayer());
}

void InkDropAnimationControllerImpl::DestroyInkDropAnimation() {
  if (!ink_drop_animation_)
    return;
  root_layer_->Remove(ink_drop_animation_->GetRootLayer());
  ink_drop_animation_.reset();
}

void InkDropAnimationControllerImpl::CreateInkDropHover() {
  DestroyInkDropHover();

  hover_ = ink_drop_host_->CreateInkDropHover();
  if (!hover_)
    return;
  root_layer_->Add(hover_->layer());
}

void InkDropAnimationControllerImpl::DestroyInkDropHover() {
  if (!hover_)
    return;
  root_layer_->Remove(hover_->layer());
  hover_.reset();
}

bool InkDropAnimationControllerImpl::IsHoverFadingInOrVisible() const {
  return hover_ && hover_->IsFadingInOrVisible();
}

void InkDropAnimationControllerImpl::AnimationStarted(
    InkDropState ink_drop_state) {}

void InkDropAnimationControllerImpl::AnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationEndedReason reason) {
  if (reason != InkDropAnimationEndedReason::SUCCESS)
    return;
  if (ShouldAnimateToHidden(ink_drop_state)) {
    ink_drop_animation_->AnimateToState(views::InkDropState::HIDDEN);
  } else if (ink_drop_state == views::InkDropState::HIDDEN) {
    if (is_hovered_)
      StartHoverAfterAnimationTimer();
    // TODO(bruthig): Investigate whether creating and destroying
    // InkDropAnimations is expensive and consider creating an
    // InkDropAnimationPool. See www.crbug.com/522175.
    DestroyInkDropAnimation();
  }
}

void InkDropAnimationControllerImpl::SetHoveredInternal(
    bool is_hovered,
    base::TimeDelta animation_duration,
    bool explode) {
  StopHoverAfterAnimationTimer();

  if (IsHoverFadingInOrVisible() == is_hovered)
    return;

  if (is_hovered) {
    CreateInkDropHover();
    if (hover_ && !IsVisible())
      hover_->FadeIn(animation_duration);
  } else {
    hover_->FadeOut(animation_duration, explode);
  }
}

void InkDropAnimationControllerImpl::StartHoverAfterAnimationTimer() {
  StopHoverAfterAnimationTimer();

  if (!hover_after_animation_timer_)
    hover_after_animation_timer_.reset(new base::OneShotTimer);

  hover_after_animation_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kHoverFadeInAfterAnimationDelayInMs),
      base::Bind(&InkDropAnimationControllerImpl::HoverAfterAnimationTimerFired,
                 base::Unretained(this)));
}

void InkDropAnimationControllerImpl::StopHoverAfterAnimationTimer() {
  if (hover_after_animation_timer_)
    hover_after_animation_timer_.reset();
}

void InkDropAnimationControllerImpl::HoverAfterAnimationTimerFired() {
  SetHoveredInternal(true, base::TimeDelta::FromMilliseconds(
                               kHoverFadeInAfterAnimationDurationInMs),
                     true);
  hover_after_animation_timer_.reset();
}

}  // namespace views
