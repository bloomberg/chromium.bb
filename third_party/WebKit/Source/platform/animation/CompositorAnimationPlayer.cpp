// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationPlayer.h"

#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "platform/animation/CompositorAnimationDelegate.h"
#include "platform/animation/CompositorKeyframeModel.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

std::unique_ptr<CompositorAnimationPlayer> CompositorAnimationPlayer::Create() {
  return std::make_unique<CompositorAnimationPlayer>(
      cc::SingleKeyframeEffectAnimationPlayer::Create(
          cc::AnimationIdProvider::NextPlayerId()));
}

std::unique_ptr<CompositorAnimationPlayer>
CompositorAnimationPlayer::CreateWorkletPlayer(
    const String& name,
    std::unique_ptr<CompositorScrollTimeline> scroll_timeline) {
  return std::make_unique<CompositorAnimationPlayer>(
      cc::WorkletAnimationPlayer::Create(
          cc::AnimationIdProvider::NextPlayerId(),
          std::string(name.Ascii().data(), name.length()),
          std::move(scroll_timeline)));
}

CompositorAnimationPlayer::CompositorAnimationPlayer(
    scoped_refptr<cc::SingleKeyframeEffectAnimationPlayer> player)
    : animation_player_(player), delegate_() {}

CompositorAnimationPlayer::~CompositorAnimationPlayer() {
  SetAnimationDelegate(nullptr);
  // Detach player from timeline, otherwise it stays there (leaks) until
  // compositor shutdown.
  if (animation_player_->animation_timeline())
    animation_player_->animation_timeline()->DetachPlayer(animation_player_);
}

cc::SingleKeyframeEffectAnimationPlayer*
CompositorAnimationPlayer::CcAnimationPlayer() const {
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

void CompositorAnimationPlayer::AddKeyframeModel(
    std::unique_ptr<CompositorKeyframeModel> keyframe_model) {
  animation_player_->AddKeyframeModel(keyframe_model->ReleaseCcKeyframeModel());
}

void CompositorAnimationPlayer::RemoveKeyframeModel(int keyframe_model_id) {
  animation_player_->RemoveKeyframeModel(keyframe_model_id);
}

void CompositorAnimationPlayer::PauseKeyframeModel(int keyframe_model_id,
                                                   double time_offset) {
  animation_player_->PauseKeyframeModel(keyframe_model_id, time_offset);
}

void CompositorAnimationPlayer::AbortKeyframeModel(int keyframe_model_id) {
  animation_player_->AbortKeyframeModel(keyframe_model_id);
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
    base::TimeTicks animation_start_time,
    std::unique_ptr<cc::AnimationCurve> curve) {
  if (delegate_) {
    delegate_->NotifyAnimationTakeover(
        (monotonic_time - base::TimeTicks()).InSecondsF(),
        (animation_start_time - base::TimeTicks()).InSecondsF(),
        std::move(curve));
  }
}

}  // namespace blink
