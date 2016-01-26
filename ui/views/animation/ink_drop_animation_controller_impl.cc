// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_animation_controller_impl.h"

#include "base/timer/timer.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/ink_drop_animation.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_hover.h"

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
const int kHoverFadeOutBeforeAnimationDurationInMs = 0;

// The amount of time in milliseconds that |hover_| should delay after a ripple
// animation before fading in.
const int kHoverFadeInAfterAnimationDelayInMs = 1000;

}  // namespace

InkDropAnimationControllerImpl::InkDropAnimationControllerImpl(
    InkDropHost* ink_drop_host)
    : ink_drop_host_(ink_drop_host),
      ink_drop_large_corner_radius_(0),
      ink_drop_small_corner_radius_(0),
      root_layer_(new ui::Layer(ui::LAYER_NOT_DRAWN)),
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

InkDropState InkDropAnimationControllerImpl::GetInkDropState() const {
  if (!ink_drop_animation_)
    return InkDropState::HIDDEN;
  return ink_drop_animation_->ink_drop_state();
}

void InkDropAnimationControllerImpl::AnimateToState(
    InkDropState ink_drop_state) {
  if (!ink_drop_animation_)
    CreateInkDropAnimation();
  ink_drop_animation_->AnimateToState(ink_drop_state);
}

void InkDropAnimationControllerImpl::SetHovered(bool is_hovered) {
  SetHoveredInternal(is_hovered,
                     is_hovered ? base::TimeDelta::FromMilliseconds(
                                      kHoverFadeInFromUserInputDurationInMs)
                                : base::TimeDelta::FromMilliseconds(
                                      kHoverFadeOutFromUserInputDurationInMs));
}

bool InkDropAnimationControllerImpl::IsHovered() const {
  return hover_ && hover_->IsVisible();
}

gfx::Size InkDropAnimationControllerImpl::GetInkDropLargeSize() const {
  return ink_drop_large_size_;
}

void InkDropAnimationControllerImpl::SetInkDropSize(const gfx::Size& large_size,
                                                    int large_corner_radius,
                                                    const gfx::Size& small_size,
                                                    int small_corner_radius) {
  // TODO(bruthig): Fix the ink drop animations to work for non-square sizes.
  DCHECK_EQ(large_size.width(), large_size.height())
      << "The ink drop animation does not currently support non-square sizes.";
  DCHECK_EQ(small_size.width(), small_size.height())
      << "The ink drop animation does not currently support non-square sizes.";
  ink_drop_large_size_ = large_size;
  ink_drop_large_corner_radius_ = large_corner_radius;
  ink_drop_small_size_ = small_size;
  ink_drop_small_corner_radius_ = small_corner_radius;

  DestroyInkDropAnimation();
  DestroyInkDropHover();
}

void InkDropAnimationControllerImpl::SetInkDropCenter(
    const gfx::Point& center_point) {
  ink_drop_center_ = center_point;
  if (ink_drop_animation_)
    ink_drop_animation_->SetCenterPoint(ink_drop_center_);
  if (hover_)
    hover_->SetCenterPoint(ink_drop_center_);
}

void InkDropAnimationControllerImpl::CreateInkDropAnimation() {
  DestroyInkDropAnimation();

  ink_drop_animation_.reset(new InkDropAnimation(
      ink_drop_large_size_, ink_drop_large_corner_radius_, ink_drop_small_size_,
      ink_drop_small_corner_radius_));

  ink_drop_animation_->AddObserver(this);
  ink_drop_animation_->SetCenterPoint(ink_drop_center_);
  root_layer_->Add(ink_drop_animation_->root_layer());
}

void InkDropAnimationControllerImpl::DestroyInkDropAnimation() {
  if (!ink_drop_animation_)
    return;
  root_layer_->Remove(ink_drop_animation_->root_layer());
  ink_drop_animation_->RemoveObserver(this);
  ink_drop_animation_.reset();
}

void InkDropAnimationControllerImpl::CreateInkDropHover() {
  DestroyInkDropHover();

  hover_.reset(
      new InkDropHover(ink_drop_small_size_, ink_drop_small_corner_radius_));
  hover_->SetCenterPoint(ink_drop_center_);
  root_layer_->Add(hover_->layer());
}

void InkDropAnimationControllerImpl::DestroyInkDropHover() {
  if (!hover_)
    return;
  root_layer_->Remove(hover_->layer());
  hover_.reset();
}

void InkDropAnimationControllerImpl::InkDropAnimationStarted(
    InkDropState ink_drop_state) {
  if (IsHovered() && ink_drop_state != views::InkDropState::HIDDEN) {
    SetHoveredInternal(false, base::TimeDelta::FromMilliseconds(
                                  kHoverFadeOutBeforeAnimationDurationInMs));
  }
}

void InkDropAnimationControllerImpl::InkDropAnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationEndedReason reason) {
  if (reason != SUCCESS)
    return;
  switch (ink_drop_state) {
    case views::InkDropState::QUICK_ACTION:
    case views::InkDropState::SLOW_ACTION:
    case views::InkDropState::DEACTIVATED:
      ink_drop_animation_->AnimateToState(views::InkDropState::HIDDEN);
      break;
    case views::InkDropState::HIDDEN:
      // TODO(bruthig): Consider only starting the timer if the InkDropHost is
      // hovered now, as oppposed to when the timer fires.
      StartHoverAfterAnimationTimer();
      // TODO(bruthig): Investigate whether creating and destroying
      // InkDropAnimations is expensive and consider creating an
      // InkDropAnimationPool. See www.crbug.com/522175.
      DestroyInkDropAnimation();
      break;
    default:
      break;
  }
}

void InkDropAnimationControllerImpl::SetHoveredInternal(
    bool is_hovered,
    base::TimeDelta animation_duration) {
  StopHoverAfterAnimationTimer();

  if (IsHovered() == is_hovered)
    return;

  if (is_hovered) {
    if (!hover_)
      CreateInkDropHover();
    if (GetInkDropState() == views::InkDropState::HIDDEN) {
      hover_->FadeIn(animation_duration);
    }
  } else {
    hover_->FadeOut(animation_duration);
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
  SetHoveredInternal(ink_drop_host_->ShouldShowInkDropHover(),
                     base::TimeDelta::FromMilliseconds(
                         kHoverFadeInAfterAnimationDurationInMs));
  hover_after_animation_timer_.reset();
}

}  // namespace views
