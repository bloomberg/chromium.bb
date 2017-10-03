// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_impl.h"

#include "base/auto_reset.h"
#include "base/memory/ptr_util.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/square_ink_drop_ripple.h"

namespace views {

namespace {

// The duration, in milliseconds for the highlight state fade in/out animations
// when it is triggered by a hover changed event.
const int kHighlightFadeInOnHoverChangeDurationMs = 250;
const int kHighlightFadeOutOnHoverChangeDurationMs = 250;

// The duration, in milliseconds for the highlight state fade in/out animations
// when it is triggered by a focus changed event.
const int kHighlightFadeInOnFocusChangeDurationMs = 0;
const int kHighlightFadeOutOnFocusChangeDurationMs = 0;

// The duration, in milliseconds, for showing/hiding the highlight when
// triggered by ripple visibility changes for the HIDE_ON_RIPPLE
// AutoHighlightMode.
const int kHighlightFadeInOnRippleHidingDurationMs = 250;
const int kHighlightFadeOutOnRippleShowingDurationMs = 120;

// The duration, in milliseconds, for showing/hiding the highlight when
// triggered by ripple visibility changes for the SHOW_ON_RIPPLE
// AutoHighlightMode.
const int kHighlightFadeInOnRippleShowingDurationMs = 250;
const int kHighlightFadeOutOnRippleHidingDurationMs = 120;

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

// HighlightState definition

InkDropImpl* InkDropImpl::HighlightState::GetInkDrop() {
  return state_factory_->ink_drop();
}

// A HighlightState to be used during InkDropImpl destruction. All event
// handlers are no-ops so as to avoid triggering animations during tear down.
class InkDropImpl::DestroyingHighlightState
    : public InkDropImpl::HighlightState {
 public:
  DestroyingHighlightState() : HighlightState(nullptr) {}

  // InkDropImpl::HighlightState:
  void Enter() override {}
  void ShowOnHoverChanged() override {}
  void OnHoverChanged() override {}
  void ShowOnFocusChanged() override {}
  void OnFocusChanged() override {}
  void AnimationStarted(InkDropState ink_drop_state) override {}
  void AnimationEnded(InkDropState ink_drop_state,
                      InkDropAnimationEndedReason reason) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DestroyingHighlightState);
};

//
// AutoHighlightMode::NONE states
//

// Animates the highlight to hidden upon entering this state. Transitions to a
// visible state based on hover/focus changes.
class InkDropImpl::NoAutoHighlightHiddenState
    : public InkDropImpl::HighlightState {
 public:
  NoAutoHighlightHiddenState(HighlightStateFactory* state_factory,
                             base::TimeDelta animation_duration,
                             bool explode);

  // InkDropImpl::HighlightState:
  void Enter() override;
  void ShowOnHoverChanged() override;
  void OnHoverChanged() override;
  void ShowOnFocusChanged() override;
  void OnFocusChanged() override;
  void AnimationStarted(InkDropState ink_drop_state) override;
  void AnimationEnded(InkDropState ink_drop_state,
                      InkDropAnimationEndedReason reason) override;

 private:
  // Handles all changes to the hover/focus status and transitions to a visible
  // state if necessary.
  void HandleHoverAndFocusChangeChanges(int animation_duration_ms);

  // The fade out animation duration.
  base::TimeDelta animation_duration_;

  // True when the highlight should explode while fading out.
  bool explode_;

  DISALLOW_COPY_AND_ASSIGN(NoAutoHighlightHiddenState);
};

// Animates the highlight to visible upon entering this state. Transitions to a
// hidden state based on hover/focus changes.
class InkDropImpl::NoAutoHighlightVisibleState
    : public InkDropImpl::HighlightState {
 public:
  NoAutoHighlightVisibleState(HighlightStateFactory* state_factory,
                              base::TimeDelta animation_duration,
                              bool explode);

  // InkDropImpl::HighlightState:
  void Enter() override;
  void Exit() override {}
  void ShowOnHoverChanged() override;
  void OnHoverChanged() override;
  void ShowOnFocusChanged() override;
  void OnFocusChanged() override;
  void AnimationStarted(InkDropState ink_drop_state) override;
  void AnimationEnded(InkDropState ink_drop_state,
                      InkDropAnimationEndedReason reason) override;

 private:
  // Handles all changes to the hover/focus status and transitions to a hidden
  // state if necessary.
  void HandleHoverAndFocusChangeChanges(int animation_duration_ms);

  // The fade in animation duration.
  base::TimeDelta animation_duration_;

  // True when the highlight should explode while fading in.
  bool explode_;

  DISALLOW_COPY_AND_ASSIGN(NoAutoHighlightVisibleState);
};

// NoAutoHighlightHiddenState definition

InkDropImpl::NoAutoHighlightHiddenState::NoAutoHighlightHiddenState(
    HighlightStateFactory* state_factory,
    base::TimeDelta animation_duration,
    bool explode)
    : InkDropImpl::HighlightState(state_factory),
      animation_duration_(animation_duration),
      explode_(explode) {}

void InkDropImpl::NoAutoHighlightHiddenState::Enter() {
  GetInkDrop()->SetHighlight(false, animation_duration_, explode_);
}

void InkDropImpl::NoAutoHighlightHiddenState::ShowOnHoverChanged() {
  HandleHoverAndFocusChangeChanges(kHighlightFadeInOnHoverChangeDurationMs);
}

void InkDropImpl::NoAutoHighlightHiddenState::OnHoverChanged() {
  HandleHoverAndFocusChangeChanges(kHighlightFadeInOnHoverChangeDurationMs);
}

void InkDropImpl::NoAutoHighlightHiddenState::ShowOnFocusChanged() {
  HandleHoverAndFocusChangeChanges(kHighlightFadeInOnFocusChangeDurationMs);
}

void InkDropImpl::NoAutoHighlightHiddenState::OnFocusChanged() {
  HandleHoverAndFocusChangeChanges(kHighlightFadeInOnFocusChangeDurationMs);
}

void InkDropImpl::NoAutoHighlightHiddenState::HandleHoverAndFocusChangeChanges(
    int animation_duration_ms) {
  if (GetInkDrop()->ShouldHighlight()) {
    GetInkDrop()->SetHighlightState(state_factory()->CreateVisibleState(
        base::TimeDelta::FromMilliseconds(animation_duration_ms), false));
  }
}

void InkDropImpl::NoAutoHighlightHiddenState::AnimationStarted(
    InkDropState ink_drop_state) {}

void InkDropImpl::NoAutoHighlightHiddenState::AnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationEndedReason reason) {}

// NoAutoHighlightVisibleState definition

InkDropImpl::NoAutoHighlightVisibleState::NoAutoHighlightVisibleState(
    HighlightStateFactory* state_factory,
    base::TimeDelta animation_duration,
    bool explode)
    : InkDropImpl::HighlightState(state_factory),
      animation_duration_(animation_duration),
      explode_(explode) {}

void InkDropImpl::NoAutoHighlightVisibleState::Enter() {
  GetInkDrop()->SetHighlight(true, animation_duration_, explode_);
}

void InkDropImpl::NoAutoHighlightVisibleState::ShowOnHoverChanged() {
  HandleHoverAndFocusChangeChanges(kHighlightFadeOutOnHoverChangeDurationMs);
}

void InkDropImpl::NoAutoHighlightVisibleState::OnHoverChanged() {
  HandleHoverAndFocusChangeChanges(kHighlightFadeOutOnHoverChangeDurationMs);
}

void InkDropImpl::NoAutoHighlightVisibleState::ShowOnFocusChanged() {
  HandleHoverAndFocusChangeChanges(kHighlightFadeOutOnFocusChangeDurationMs);
}

void InkDropImpl::NoAutoHighlightVisibleState::OnFocusChanged() {
  HandleHoverAndFocusChangeChanges(kHighlightFadeOutOnFocusChangeDurationMs);
}

void InkDropImpl::NoAutoHighlightVisibleState::HandleHoverAndFocusChangeChanges(
    int animation_duration_ms) {
  if (!GetInkDrop()->ShouldHighlight()) {
    GetInkDrop()->SetHighlightState(state_factory()->CreateHiddenState(
        base::TimeDelta::FromMilliseconds(animation_duration_ms), false));
  }
}

void InkDropImpl::NoAutoHighlightVisibleState::AnimationStarted(
    InkDropState ink_drop_state) {}

void InkDropImpl::NoAutoHighlightVisibleState::AnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationEndedReason reason) {}

//
// AutoHighlightMode::HIDE_ON_RIPPLE states
//

// Extends the base hidden state to re-show the highlight after the ripple
// becomes hidden.
class InkDropImpl::HideHighlightOnRippleHiddenState
    : public InkDropImpl::NoAutoHighlightHiddenState {
 public:
  HideHighlightOnRippleHiddenState(HighlightStateFactory* state_factory,
                                   base::TimeDelta animation_duration,
                                   bool explode);

  // InkDropImpl::NoAutoHighlightHiddenState:
  void ShowOnHoverChanged() override;
  void OnHoverChanged() override;
  void ShowOnFocusChanged() override;
  void OnFocusChanged() override;
  void AnimationStarted(InkDropState ink_drop_state) override;
  void AnimationEnded(InkDropState ink_drop_state,
                      InkDropAnimationEndedReason reason) override;

 private:
  // Starts the |highlight_after_ripple_timer_|. This will stop the current
  // |highlight_after_ripple_timer_| instance if it exists.
  void StartHighlightAfterRippleTimer();

  // Callback for when the |highlight_after_ripple_timer_| fires. Transitions to
  // a visible state if the ink drop should be highlighted.
  void HighlightAfterRippleTimerFired();

  // The timer used to delay the highlight fade in after an ink drop ripple
  // animation.
  std::unique_ptr<base::Timer> highlight_after_ripple_timer_;

  DISALLOW_COPY_AND_ASSIGN(HideHighlightOnRippleHiddenState);
};

// Extends the base visible state to hide the highlight when the ripple becomes
// visible.
class InkDropImpl::HideHighlightOnRippleVisibleState
    : public InkDropImpl::NoAutoHighlightVisibleState {
 public:
  HideHighlightOnRippleVisibleState(HighlightStateFactory* state_factory,
                                    base::TimeDelta animation_duration,
                                    bool explode);

  // InkDropImpl::NoAutoHighlightVisibleState:
  void AnimationStarted(InkDropState ink_drop_state) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HideHighlightOnRippleVisibleState);
};

// HideHighlightOnRippleHiddenState definition

InkDropImpl::HideHighlightOnRippleHiddenState::HideHighlightOnRippleHiddenState(
    HighlightStateFactory* state_factory,
    base::TimeDelta animation_duration,
    bool explode)
    : InkDropImpl::NoAutoHighlightHiddenState(state_factory,
                                              animation_duration,
                                              explode),
      highlight_after_ripple_timer_(nullptr) {}

void InkDropImpl::HideHighlightOnRippleHiddenState::ShowOnHoverChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightHiddenState::ShowOnHoverChanged();
}

void InkDropImpl::HideHighlightOnRippleHiddenState::OnHoverChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightHiddenState::OnHoverChanged();
}

void InkDropImpl::HideHighlightOnRippleHiddenState::ShowOnFocusChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightHiddenState::ShowOnFocusChanged();
}

void InkDropImpl::HideHighlightOnRippleHiddenState::OnFocusChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightHiddenState::OnFocusChanged();
}

void InkDropImpl::HideHighlightOnRippleHiddenState::AnimationStarted(
    InkDropState ink_drop_state) {
  if (ink_drop_state == views::InkDropState::DEACTIVATED &&
      GetInkDrop()->ShouldHighlightBasedOnFocus()) {
    // It's possible to get animation started events when destroying the
    // |ink_drop_ripple_|.
    // TODO(bruthig): Investigate if the animation framework can address this
    // issue instead. See https://crbug.com/663335.
    if (GetInkDrop()->ink_drop_ripple_)
      GetInkDrop()->ink_drop_ripple_->HideImmediately();
    GetInkDrop()->SetHighlightState(
        state_factory()->CreateVisibleState(base::TimeDelta(), false));
  }
}

void InkDropImpl::HideHighlightOnRippleHiddenState::AnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationEndedReason reason) {
  if (ink_drop_state == InkDropState::HIDDEN) {
    // Re-highlight, as necessary. For hover, there's a delay; for focus, jump
    // straight into the animation.
    if (GetInkDrop()->ShouldHighlightBasedOnFocus()) {
      GetInkDrop()->SetHighlightState(
          state_factory()->CreateVisibleState(base::TimeDelta(), false));
      return;
    } else {
      StartHighlightAfterRippleTimer();
    }
  }
}

void InkDropImpl::HideHighlightOnRippleHiddenState::
    StartHighlightAfterRippleTimer() {
  highlight_after_ripple_timer_.reset(new base::OneShotTimer);
  highlight_after_ripple_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kHoverFadeInAfterRippleDelayMs),
      base::Bind(&InkDropImpl::HideHighlightOnRippleHiddenState::
                     HighlightAfterRippleTimerFired,
                 base::Unretained(this)));
}

void InkDropImpl::HideHighlightOnRippleHiddenState::
    HighlightAfterRippleTimerFired() {
  highlight_after_ripple_timer_.reset();
  if (GetInkDrop()->GetTargetInkDropState() == InkDropState::HIDDEN &&
      GetInkDrop()->ShouldHighlight()) {
    GetInkDrop()->SetHighlightState(state_factory()->CreateVisibleState(
        base::TimeDelta::FromMilliseconds(
            kHighlightFadeInOnRippleHidingDurationMs),
        true));
  }
}

// HideHighlightOnRippleVisibleState definition

InkDropImpl::HideHighlightOnRippleVisibleState::
    HideHighlightOnRippleVisibleState(HighlightStateFactory* state_factory,
                                      base::TimeDelta animation_duration,
                                      bool explode)
    : InkDropImpl::NoAutoHighlightVisibleState(state_factory,
                                               animation_duration,
                                               explode) {}

void InkDropImpl::HideHighlightOnRippleVisibleState::AnimationStarted(
    InkDropState ink_drop_state) {
  if (ink_drop_state != InkDropState::HIDDEN) {
    GetInkDrop()->SetHighlightState(state_factory()->CreateHiddenState(
        base::TimeDelta::FromMilliseconds(
            kHighlightFadeOutOnRippleShowingDurationMs),
        true));
  }
}

//
// AutoHighlightMode::SHOW_ON_RIPPLE states
//

// Extends the base hidden state to show the highlight when the ripple becomes
// visible.
class InkDropImpl::ShowHighlightOnRippleHiddenState
    : public InkDropImpl::NoAutoHighlightHiddenState {
 public:
  ShowHighlightOnRippleHiddenState(HighlightStateFactory* state_factory,
                                   base::TimeDelta animation_duration,
                                   bool explode);

  // InkDropImpl::NoAutoHighlightHiddenState:
  void AnimationStarted(InkDropState ink_drop_state) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShowHighlightOnRippleHiddenState);
};

// Extends the base visible state to hide the highlight when the ripple becomes
// hidden.
class InkDropImpl::ShowHighlightOnRippleVisibleState
    : public InkDropImpl::NoAutoHighlightVisibleState {
 public:
  ShowHighlightOnRippleVisibleState(HighlightStateFactory* state_factory,
                                    base::TimeDelta animation_duration,
                                    bool explode);

  // InkDropImpl::NoAutoHighlightVisibleState:
  void ShowOnHoverChanged() override;
  void OnHoverChanged() override;
  void ShowOnFocusChanged() override;
  void OnFocusChanged() override;
  void AnimationStarted(InkDropState ink_drop_state) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShowHighlightOnRippleVisibleState);
};

// ShowHighlightOnRippleHiddenState definition

InkDropImpl::ShowHighlightOnRippleHiddenState::ShowHighlightOnRippleHiddenState(
    HighlightStateFactory* state_factory,
    base::TimeDelta animation_duration,
    bool explode)
    : InkDropImpl::NoAutoHighlightHiddenState(state_factory,
                                              animation_duration,
                                              explode) {}

void InkDropImpl::ShowHighlightOnRippleHiddenState::AnimationStarted(
    InkDropState ink_drop_state) {
  if (ink_drop_state != views::InkDropState::HIDDEN) {
    GetInkDrop()->SetHighlightState(state_factory()->CreateVisibleState(
        base::TimeDelta::FromMilliseconds(
            kHighlightFadeInOnRippleShowingDurationMs),
        false));
  }
}

// ShowHighlightOnRippleVisibleState definition

InkDropImpl::ShowHighlightOnRippleVisibleState::
    ShowHighlightOnRippleVisibleState(HighlightStateFactory* state_factory,
                                      base::TimeDelta animation_duration,
                                      bool explode)
    : InkDropImpl::NoAutoHighlightVisibleState(state_factory,
                                               animation_duration,
                                               explode) {}

void InkDropImpl::ShowHighlightOnRippleVisibleState::ShowOnHoverChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightVisibleState::ShowOnHoverChanged();
}

void InkDropImpl::ShowHighlightOnRippleVisibleState::OnHoverChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightVisibleState::OnHoverChanged();
}

void InkDropImpl::ShowHighlightOnRippleVisibleState::ShowOnFocusChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightVisibleState::ShowOnFocusChanged();
}

void InkDropImpl::ShowHighlightOnRippleVisibleState::OnFocusChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightVisibleState::OnFocusChanged();
}

void InkDropImpl::ShowHighlightOnRippleVisibleState::AnimationStarted(
    InkDropState ink_drop_state) {
  if (ink_drop_state == InkDropState::HIDDEN &&
      !GetInkDrop()->ShouldHighlight()) {
    GetInkDrop()->SetHighlightState(state_factory()->CreateHiddenState(
        base::TimeDelta::FromMilliseconds(
            kHighlightFadeOutOnRippleHidingDurationMs),
        false));
  }
}

InkDropImpl::HighlightStateFactory::HighlightStateFactory(
    InkDropImpl::AutoHighlightMode highlight_mode,
    InkDropImpl* ink_drop)
    : highlight_mode_(highlight_mode), ink_drop_(ink_drop) {}

std::unique_ptr<InkDropImpl::HighlightState>
InkDropImpl::HighlightStateFactory::CreateStartState() {
  switch (highlight_mode_) {
    case InkDropImpl::AutoHighlightMode::NONE:
      return base::MakeUnique<NoAutoHighlightHiddenState>(
          this, base::TimeDelta(), false);
    case InkDropImpl::AutoHighlightMode::HIDE_ON_RIPPLE:
      return base::MakeUnique<HideHighlightOnRippleHiddenState>(
          this, base::TimeDelta(), false);
    case InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE:
      return base::MakeUnique<ShowHighlightOnRippleHiddenState>(
          this, base::TimeDelta(), false);
  }
  // Required for some compilers.
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<InkDropImpl::HighlightState>
InkDropImpl::HighlightStateFactory::CreateHiddenState(
    base::TimeDelta animation_duration,
    bool explode) {
  switch (highlight_mode_) {
    case InkDropImpl::AutoHighlightMode::NONE:
      return base::MakeUnique<NoAutoHighlightHiddenState>(
          this, animation_duration, explode);
    case InkDropImpl::AutoHighlightMode::HIDE_ON_RIPPLE:
      return base::MakeUnique<HideHighlightOnRippleHiddenState>(
          this, animation_duration, explode);
    case InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE:
      return base::MakeUnique<ShowHighlightOnRippleHiddenState>(
          this, animation_duration, explode);
  }
  // Required for some compilers.
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<InkDropImpl::HighlightState>
InkDropImpl::HighlightStateFactory::CreateVisibleState(
    base::TimeDelta animation_duration,
    bool explode) {
  switch (highlight_mode_) {
    case InkDropImpl::AutoHighlightMode::NONE:
      return base::MakeUnique<NoAutoHighlightVisibleState>(
          this, animation_duration, explode);
    case InkDropImpl::AutoHighlightMode::HIDE_ON_RIPPLE:
      return base::MakeUnique<HideHighlightOnRippleVisibleState>(
          this, animation_duration, explode);
    case InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE:
      return base::MakeUnique<ShowHighlightOnRippleVisibleState>(
          this, animation_duration, explode);
  }
  // Required for some compilers.
  NOTREACHED();
  return nullptr;
}

InkDropImpl::InkDropImpl(InkDropHost* ink_drop_host, const gfx::Size& host_size)
    : ink_drop_host_(ink_drop_host),
      root_layer_(new ui::Layer(ui::LAYER_NOT_DRAWN)),
      root_layer_added_to_host_(false),
      show_highlight_on_hover_(true),
      show_highlight_on_focus_(false),
      is_hovered_(false),
      is_focused_(false),
      exiting_highlight_state_(false),
      destroying_(false) {
  root_layer_->SetBounds(gfx::Rect(host_size));
  SetAutoHighlightMode(AutoHighlightMode::NONE);
  root_layer_->set_name("InkDropImpl:RootLayer");
}

InkDropImpl::~InkDropImpl() {
  destroying_ = true;
  // Setting a no-op state prevents animations from being triggered on a null
  // |ink_drop_ripple_| as a side effect of the tear down.
  SetHighlightState(base::MakeUnique<DestroyingHighlightState>());

  // Explicitly destroy the InkDropRipple so that this still exists if
  // views::InkDropRippleObserver methods are called on this.
  DestroyInkDropRipple();
  DestroyInkDropHighlight();
}

void InkDropImpl::SetAutoHighlightMode(AutoHighlightMode auto_highlight_mode) {
  // Exit the current state completely first in case state tear down accesses
  // the current |highlight_state_factory_| instance.
  ExitHighlightState();
  highlight_state_factory_ =
      base::MakeUnique<HighlightStateFactory>(auto_highlight_mode, this);
  SetHighlightState(highlight_state_factory_->CreateStartState());
}

void InkDropImpl::HostSizeChanged(const gfx::Size& new_size) {
  // |root_layer_| should fill the entire host because it affects the clipping
  // when a mask layer is applied to it. This will not affect clipping if no
  // mask layer is set.
  root_layer_->SetBounds(gfx::Rect(new_size));
  if (ink_drop_ripple_)
    ink_drop_ripple_->HostSizeChanged(new_size);
}

InkDropState InkDropImpl::GetTargetInkDropState() const {
  if (!ink_drop_ripple_)
    return InkDropState::HIDDEN;
  return ink_drop_ripple_->target_ink_drop_state();
}

void InkDropImpl::AnimateToState(InkDropState ink_drop_state) {
  // Never animate hidden -> hidden, since that will add layers which may never
  // be needed. Other same-state transitions may restart animations.
  if (ink_drop_state == InkDropState::HIDDEN &&
      GetTargetInkDropState() == InkDropState::HIDDEN)
    return;

  DestroyHiddenTargetedAnimations();
  if (!ink_drop_ripple_)
    CreateInkDropRipple();
  ink_drop_ripple_->AnimateToState(ink_drop_state);
}

void InkDropImpl::SnapToActivated() {
  DestroyHiddenTargetedAnimations();
  if (!ink_drop_ripple_)
    CreateInkDropRipple();
  ink_drop_ripple_->SnapToActivated();
}

void InkDropImpl::SetHovered(bool is_hovered) {
  is_hovered_ = is_hovered;
  highlight_state_->OnHoverChanged();
}

void InkDropImpl::SetFocused(bool is_focused) {
  is_focused_ = is_focused;
  highlight_state_->OnFocusChanged();
}

bool InkDropImpl::IsHighlightFadingInOrVisible() const {
  return highlight_ && highlight_->IsFadingInOrVisible();
}

void InkDropImpl::SetShowHighlightOnHover(bool show_highlight_on_hover) {
  show_highlight_on_hover_ = show_highlight_on_hover;
  highlight_state_->ShowOnHoverChanged();
}

void InkDropImpl::SetShowHighlightOnFocus(bool show_highlight_on_focus) {
  show_highlight_on_focus_ = show_highlight_on_focus;
  highlight_state_->ShowOnFocusChanged();
}

void InkDropImpl::DestroyHiddenTargetedAnimations() {
  if (ink_drop_ripple_ &&
      (ink_drop_ripple_->target_ink_drop_state() == InkDropState::HIDDEN ||
       ShouldAnimateToHidden(ink_drop_ripple_->target_ink_drop_state()))) {
    DestroyInkDropRipple();
  }
}

void InkDropImpl::CreateInkDropRipple() {
  DCHECK(!destroying_);

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
  DCHECK(!destroying_);

  DestroyInkDropHighlight();

  highlight_ = ink_drop_host_->CreateInkDropHighlight();
  DCHECK(highlight_);

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

// -----------------------------------------------------------------------------
// views::InkDropRippleObserver:

void InkDropImpl::AnimationStarted(InkDropState ink_drop_state) {
  highlight_state_->AnimationStarted(ink_drop_state);
  NotifyInkDropAnimationStarted();
}

void InkDropImpl::AnimationEnded(InkDropState ink_drop_state,
                                 InkDropAnimationEndedReason reason) {
  highlight_state_->AnimationEnded(ink_drop_state, reason);
  NotifyInkDropRippleAnimationEnded(ink_drop_state);
  if (reason != InkDropAnimationEndedReason::SUCCESS)
    return;
  // |ink_drop_ripple_| might be null during destruction.
  if (!ink_drop_ripple_)
    return;
  if (ShouldAnimateToHidden(ink_drop_state)) {
    ink_drop_ripple_->AnimateToState(views::InkDropState::HIDDEN);
  } else if (ink_drop_state == views::InkDropState::HIDDEN) {
    // TODO(bruthig): Investigate whether creating and destroying
    // InkDropRipples is expensive and consider creating an
    // InkDropRipplePool. See www.crbug.com/522175.
    DestroyInkDropRipple();
  }
}

// -----------------------------------------------------------------------------
// views::InkDropHighlightObserver:

void InkDropImpl::AnimationStarted(
    InkDropHighlight::AnimationType animation_type) {
  NotifyInkDropAnimationStarted();
}

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
  if (IsHighlightFadingInOrVisible() == should_highlight)
    return;

  if (should_highlight) {
    CreateInkDropHighlight();
    highlight_->FadeIn(animation_duration);
  } else {
    highlight_->FadeOut(animation_duration, explode);
  }
}

bool InkDropImpl::ShouldHighlight() const {
  return ShouldHighlightBasedOnFocus() ||
         (show_highlight_on_hover_ && is_hovered_);
}

bool InkDropImpl::ShouldHighlightBasedOnFocus() const {
  return show_highlight_on_focus_ && is_focused_;
}

void InkDropImpl::SetHighlightState(
    std::unique_ptr<HighlightState> highlight_state) {
  ExitHighlightState();
  highlight_state_ = std::move(highlight_state);
  highlight_state_->Enter();
}

void InkDropImpl::ExitHighlightState() {
  DCHECK(!exiting_highlight_state_) << "HighlightStates should not be changed "
                                       "within a call to "
                                       "HighlightState::Exit().";
  if (highlight_state_) {
    base::AutoReset<bool> exit_guard(&exiting_highlight_state_, true);
    highlight_state_->Exit();
  }
  highlight_state_ = nullptr;
}

}  // namespace views
