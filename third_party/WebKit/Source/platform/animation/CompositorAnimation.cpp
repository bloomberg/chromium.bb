// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimation.h"

#include "base/memory/ptr_util.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "platform/animation/CompositorAnimationCurve.h"
#include "platform/animation/CompositorFilterAnimationCurve.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/animation/CompositorScrollOffsetAnimationCurve.h"
#include "platform/animation/CompositorTransformAnimationCurve.h"
#include <memory>

using cc::Animation;
using cc::AnimationIdProvider;

using blink::CompositorAnimation;
using blink::CompositorAnimationCurve;

namespace blink {

CompositorAnimation::CompositorAnimation(
    const CompositorAnimationCurve& curve,
    CompositorTargetProperty::Type target_property,
    int animation_id,
    int group_id) {
  if (!animation_id)
    animation_id = AnimationIdProvider::NextAnimationId();
  if (!group_id)
    group_id = AnimationIdProvider::NextGroupId();

  animation_ = Animation::Create(curve.CloneToAnimationCurve(), animation_id,
                                 group_id, target_property);
}

CompositorAnimation::~CompositorAnimation() {}

int CompositorAnimation::Id() const {
  return animation_->id();
}

int CompositorAnimation::Group() const {
  return animation_->group();
}

CompositorTargetProperty::Type CompositorAnimation::TargetProperty() const {
  return static_cast<CompositorTargetProperty::Type>(
      animation_->target_property_id());
}

double CompositorAnimation::Iterations() const {
  return animation_->iterations();
}

void CompositorAnimation::SetIterations(double n) {
  animation_->set_iterations(n);
}

double CompositorAnimation::IterationStart() const {
  return animation_->iteration_start();
}

void CompositorAnimation::SetIterationStart(double iteration_start) {
  animation_->set_iteration_start(iteration_start);
}

double CompositorAnimation::StartTime() const {
  return (animation_->start_time() - base::TimeTicks()).InSecondsF();
}

void CompositorAnimation::SetStartTime(double monotonic_time) {
  animation_->set_start_time(base::TimeTicks::FromInternalValue(
      monotonic_time * base::Time::kMicrosecondsPerSecond));
}

double CompositorAnimation::TimeOffset() const {
  return animation_->time_offset().InSecondsF();
}

void CompositorAnimation::SetTimeOffset(double monotonic_time) {
  animation_->set_time_offset(base::TimeDelta::FromSecondsD(monotonic_time));
}

blink::CompositorAnimation::Direction CompositorAnimation::GetDirection()
    const {
  return animation_->direction();
}

void CompositorAnimation::SetDirection(Direction direction) {
  animation_->set_direction(direction);
}

double CompositorAnimation::PlaybackRate() const {
  return animation_->playback_rate();
}

void CompositorAnimation::SetPlaybackRate(double playback_rate) {
  animation_->set_playback_rate(playback_rate);
}

blink::CompositorAnimation::FillMode CompositorAnimation::GetFillMode() const {
  return animation_->fill_mode();
}

void CompositorAnimation::SetFillMode(FillMode fill_mode) {
  animation_->set_fill_mode(fill_mode);
}

std::unique_ptr<cc::Animation> CompositorAnimation::ReleaseCcAnimation() {
  animation_->set_needs_synchronized_start_time(true);
  return std::move(animation_);
}

std::unique_ptr<CompositorFloatAnimationCurve>
CompositorAnimation::FloatCurveForTesting() const {
  const cc::AnimationCurve* curve = animation_->curve();
  DCHECK_EQ(cc::AnimationCurve::FLOAT, curve->Type());

  auto keyframed_curve = base::WrapUnique(
      static_cast<cc::KeyframedFloatAnimationCurve*>(curve->Clone().release()));
  return CompositorFloatAnimationCurve::CreateForTesting(
      std::move(keyframed_curve));
}

}  // namespace blink
