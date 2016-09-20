// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_impl.h"

#include "base/auto_reset.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/square_ink_drop_ripple.h"

namespace views {

namespace {

// The duration, in milliseconds, of the highlight state fade in animation when
// it is triggered by user input.
const int kHighlightFadeInFromUserInputDurationMs = 250;

// The duration, in milliseconds, of the highlight state fade out animation when
// it is triggered by user input.
const int kHighlightFadeOutFromUserInputDurationMs = 250;

// The duration, in milliseconds, of the highlight state fade in animation when
// it is triggered by an ink drop ripple animation ending.
const int kHighlightFadeInAfterRippleDurationMs = 250;

// The duration, in milliseconds, of the highlight state fade out animation when
// it is triggered by an ink drop ripple animation starting.
const int kHighlightFadeOutBeforeRippleDurationMs = 120;

// The amount of time in milliseconds that |highlight_| should delay after a
// ripple animation before fading in, for highlight due to mouse hover.
const int kHoverFadeInAfterRippleDelayMs = 1000;

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

InkDropImpl::InkDropImpl(InkDropHost* ink_drop_host)
    : ink_drop_host_(ink_drop_host),
      root_layer_(new ui::Layer(ui::LAYER_NOT_DRAWN)),
      root_layer_added_to_host_(false),
      is_hovered_(false),
      is_focused_(false),
      highlight_after_ripple_timer_(nullptr) {
  root_layer_->set_name("InkDropImpl:RootLayer");
}

InkDropImpl::~InkDropImpl() {
  // Explicitly destroy the InkDropRipple so that this still exists if
  // views::InkDropRippleObserver methods are called on this.
  DestroyInkDropRipple();
  DestroyInkDropHighlight();
}

InkDropState InkDropImpl::GetTargetInkDropState() const {
  if (!ink_drop_ripple_)
    return InkDropState::HIDDEN;
  return ink_drop_ripple_->target_ink_drop_state();
}

void InkDropImpl::AnimateToState(InkDropState ink_drop_state) {
  DestroyHiddenTargetedAnimations();
  if (!ink_drop_ripple_)
    CreateInkDropRipple();

  if (ink_drop_ripple_->OverridesHighlight()) {
    // When deactivating and the host is focused, snap back to the highlight
    // state. (In the case of highlighting due to hover, we'll animate the
    // highlight back in after a delay.)
    if (ink_drop_state == views::InkDropState::DEACTIVATED && is_focused_) {
      ink_drop_ripple_->HideImmediately();
      SetHighlight(true, base::TimeDelta(), false);
      return;
    }

    if (ink_drop_state != views::InkDropState::HIDDEN) {
      SetHighlight(false, base::TimeDelta::FromMilliseconds(
                              kHighlightFadeOutBeforeRippleDurationMs),
                   true);
    }
  }

  ink_drop_ripple_->AnimateToState(ink_drop_state);
}

void InkDropImpl::SnapToActivated() {
  DestroyHiddenTargetedAnimations();
  if (!ink_drop_ripple_)
    CreateInkDropRipple();

  if (ink_drop_ripple_->OverridesHighlight())
    SetHighlight(false, base::TimeDelta(), false);

  ink_drop_ripple_->SnapToActivated();
}

void InkDropImpl::SetHovered(bool is_hovered) {
  is_hovered_ = is_hovered;
  SetHighlight(ShouldHighlight(),
               ShouldHighlight()
                   ? base::TimeDelta::FromMilliseconds(
                         kHighlightFadeInFromUserInputDurationMs)
                   : base::TimeDelta::FromMilliseconds(
                         kHighlightFadeOutFromUserInputDurationMs),
               false);
}

void InkDropImpl::SetFocused(bool is_focused) {
  is_focused_ = is_focused;
  SetHighlight(ShouldHighlight(), base::TimeDelta(), false);
}

void InkDropImpl::DestroyHiddenTargetedAnimations() {
  if (ink_drop_ripple_ &&
      (ink_drop_ripple_->target_ink_drop_state() == InkDropState::HIDDEN ||
       ShouldAnimateToHidden(ink_drop_ripple_->target_ink_drop_state()))) {
    DestroyInkDropRipple();
  }
}

void InkDropImpl::CreateInkDropRipple() {
  DestroyInkDropRipple();
  ink_drop_ripple_ = ink_drop_host_->CreateInkDropRipple();
  ink_drop_ripple_->set_observer(this);
  root_layer_->Add(ink_drop_ripple_->GetRootLayer());
  AddRootLayerToHostIfNeeded();
}

void InkDropImpl::DestroyInkDropRipple() {
  if (!ink_drop_ripple_)
    return;
  root_layer_->Remove(ink_drop_ripple_->GetRootLayer());
  ink_drop_ripple_.reset();
  RemoveRootLayerFromHostIfNeeded();
}

void InkDropImpl::CreateInkDropHighlight() {
  DestroyInkDropHighlight();

  highlight_ = ink_drop_host_->CreateInkDropHighlight();
  if (!highlight_)
    return;
  highlight_->set_observer(this);
  root_layer_->Add(highlight_->layer());
  AddRootLayerToHostIfNeeded();
}

void InkDropImpl::DestroyInkDropHighlight() {
  if (!highlight_)
    return;
  root_layer_->Remove(highlight_->layer());
  highlight_->set_observer(nullptr);
  highlight_.reset();
  RemoveRootLayerFromHostIfNeeded();
}

void InkDropImpl::AddRootLayerToHostIfNeeded() {
  DCHECK(highlight_ || ink_drop_ripple_);
  DCHECK(!root_layer_->children().empty());
  if (!root_layer_added_to_host_) {
    root_layer_added_to_host_ = true;
    ink_drop_host_->AddInkDropLayer(root_layer_.get());
  }
}

void InkDropImpl::RemoveRootLayerFromHostIfNeeded() {
  if (root_layer_added_to_host_ && !highlight_ && !ink_drop_ripple_) {
    root_layer_added_to_host_ = false;
    ink_drop_host_->RemoveInkDropLayer(root_layer_.get());
  }
}

bool InkDropImpl::IsHighlightFadingInOrVisible() const {
  return highlight_ && highlight_->IsFadingInOrVisible();
}

// -----------------------------------------------------------------------------
// views::InkDropRippleObserver:

void InkDropImpl::AnimationStarted(InkDropState ink_drop_state) {}

void InkDropImpl::AnimationEnded(InkDropState ink_drop_state,
                                 InkDropAnimationEndedReason reason) {
  if (reason != InkDropAnimationEndedReason::SUCCESS)
    return;
  if (ShouldAnimateToHidden(ink_drop_state)) {
    ink_drop_ripple_->AnimateToState(views::InkDropState::HIDDEN);
  } else if (ink_drop_state == views::InkDropState::HIDDEN) {
    // Re-highlight, as necessary. For hover, there's a delay; for focus, jump
    // straight into the animation.
    if (!IsHighlightFadingInOrVisible()) {
      if (is_focused_)
        HighlightAfterRippleTimerFired();
      else if (is_hovered_)
        StartHighlightAfterRippleTimer();
    }

    // TODO(bruthig): Investigate whether creating and destroying
    // InkDropRipples is expensive and consider creating an
    // InkDropRipplePool. See www.crbug.com/522175.
    DestroyInkDropRipple();
  }
}

// -----------------------------------------------------------------------------
// views::InkDropHighlightObserver:

void InkDropImpl::AnimationStarted(
    InkDropHighlight::AnimationType animation_type) {}

void InkDropImpl::AnimationEnded(InkDropHighlight::AnimationType animation_type,
                                 InkDropAnimationEndedReason reason) {
  if (animation_type == InkDropHighlight::FADE_OUT &&
      reason == InkDropAnimationEndedReason::SUCCESS) {
    DestroyInkDropHighlight();
  }
}

void InkDropImpl::SetHighlight(bool should_highlight,
                               base::TimeDelta animation_duration,
                               bool explode) {
  highlight_after_ripple_timer_.reset();

  if (IsHighlightFadingInOrVisible() == should_highlight)
    return;

  if (should_highlight) {
    CreateInkDropHighlight();
    if (highlight_ &&
        !(ink_drop_ripple_ && ink_drop_ripple_->IsVisible() &&
          ink_drop_ripple_->OverridesHighlight())) {
      highlight_->FadeIn(animation_duration);
    }
  } else {
    highlight_->FadeOut(animation_duration, explode);
  }
}

bool InkDropImpl::ShouldHighlight() const {
  return is_focused_ || is_hovered_;
}

void InkDropImpl::StartHighlightAfterRippleTimer() {
  highlight_after_ripple_timer_.reset(new base::OneShotTimer);
  highlight_after_ripple_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kHoverFadeInAfterRippleDelayMs),
      base::Bind(&InkDropImpl::HighlightAfterRippleTimerFired,
                 base::Unretained(this)));
}

void InkDropImpl::HighlightAfterRippleTimerFired() {
  SetHighlight(true, base::TimeDelta::FromMilliseconds(
                         kHighlightFadeInAfterRippleDurationMs),
               true);
  highlight_after_ripple_timer_.reset();
}

}  // namespace views
