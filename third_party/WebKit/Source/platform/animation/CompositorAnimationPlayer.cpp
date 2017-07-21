// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationPlayer.h"

#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorAnimationDelegate.h"

namespace blink {

CompositorAnimationPlayer::CompositorAnimationPlayer()
    : animation_player_(
          cc::AnimationPlayer::Create(cc::AnimationIdProvider::NextPlayerId())),
      delegate_() {}

CompositorAnimationPlayer::~CompositorAnimationPlayer() {
  SetAnimationDelegate(nullptr);
  // Detach player from timeline, otherwise it stays there (leaks) until
  // compositor shutdown.
  if (animation_player_->animation_timeline())
    animation_player_->animation_timeline()->DetachPlayer(animation_player_);
}

cc::AnimationPlayer* CompositorAnimationPlayer::CcAnimationPlayer() const {
  return animation_player_.get();
}

void CompositorAnimationPlayer::SetAnimationDelegate(
    CompositorAnimationDelegate* delegate) {
  delegate_ = delegate;
  animation_player_->set_animation_delegate(delegate ? this : nullptr);
}

void CompositorAnimationPlayer::AttachElement(const CompositorElementId& id) {
  animation_player_->AttachElement(id);
}

void CompositorAnimationPlayer::DetachElement() {
  animation_player_->DetachElement();
}

bool CompositorAnimationPlayer::IsElementAttached() const {
  return !!animation_player_->element_id();
}

void CompositorAnimationPlayer::AddAnimation(
    std::unique_ptr<CompositorAnimation> animation) {
  animation_player_->AddAnimation(animation->ReleaseCcAnimation());
}

void CompositorAnimationPlayer::RemoveAnimation(int animation_id) {
  animation_player_->RemoveAnimation(animation_id);
}

void CompositorAnimationPlayer::PauseAnimation(int animation_id,
                                               double time_offset) {
  animation_player_->PauseAnimation(animation_id, time_offset);
}

void CompositorAnimationPlayer::AbortAnimation(int animation_id) {
  animation_player_->AbortAnimation(animation_id);
}

void CompositorAnimationPlayer::NotifyAnimationStarted(
    base::TimeTicks monotonic_time,
    int target_property,
    int group) {
  if (delegate_)
    delegate_->NotifyAnimationStarted(
        (monotonic_time - base::TimeTicks()).InSecondsF(), group);
}

void CompositorAnimationPlayer::NotifyAnimationFinished(
    base::TimeTicks monotonic_time,
    int target_property,
    int group) {
  if (delegate_)
    delegate_->NotifyAnimationFinished(
        (monotonic_time - base::TimeTicks()).InSecondsF(), group);
}

void CompositorAnimationPlayer::NotifyAnimationAborted(
    base::TimeTicks monotonic_time,
    int target_property,
    int group) {
  if (delegate_)
    delegate_->NotifyAnimationAborted(
        (monotonic_time - base::TimeTicks()).InSecondsF(), group);
}

void CompositorAnimationPlayer::NotifyAnimationTakeover(
    base::TimeTicks monotonic_time,
    int target_property,
    double animation_start_time,
    std::unique_ptr<cc::AnimationCurve> curve) {
  if (delegate_) {
    delegate_->NotifyAnimationTakeover(
        (monotonic_time - base::TimeTicks()).InSecondsF(), animation_start_time,
        std::move(curve));
  }
}

}  // namespace blink
