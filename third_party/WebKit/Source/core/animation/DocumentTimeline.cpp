/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/animation/DocumentTimeline.h"

#include <algorithm>
#include "core/animation/Animation.h"
#include "core/animation/AnimationClock.h"
#include "core/animation/DocumentTimelineOptions.h"
#include "core/animation/ElementAnimations.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrameView.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Page.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"

namespace blink {

namespace {

bool CompareAnimations(const Member<Animation>& left,
                       const Member<Animation>& right) {
  return Animation::HasLowerPriority(left.Get(), right.Get());
}
}  // namespace

// This value represents 1 frame at 30Hz plus a little bit of wiggle room.
// TODO: Plumb a nominal framerate through and derive this value from that.
const double DocumentTimeline::kMinimumDelay = 0.04;

DocumentTimeline* DocumentTimeline::Create(Document* document,
                                           double origin_time_in_milliseconds,
                                           PlatformTiming* timing) {
  return new DocumentTimeline(document, origin_time_in_milliseconds, timing);
}

DocumentTimeline* DocumentTimeline::Create(
    ExecutionContext* execution_context,
    const DocumentTimelineOptions& options) {
  Document* document = ToDocument(execution_context);
  return new DocumentTimeline(document, options.originTime(), nullptr);
}

DocumentTimeline::DocumentTimeline(Document* document,
                                   double origin_time_in_milliseconds,
                                   PlatformTiming* timing)
    : document_(document),
      origin_time_(origin_time_in_milliseconds / 1000),
      zero_time_(origin_time_),
      zero_time_initialized_(false),
      outdated_animation_count_(0),
      playback_rate_(1),
      last_current_time_internal_(0) {
  if (!timing)
    timing_ = new DocumentTimelineTiming(this);
  else
    timing_ = timing;

  if (Platform::Current()->IsThreadedAnimationEnabled())
    compositor_timeline_ = CompositorAnimationTimeline::Create();

  DCHECK(document);
}

bool DocumentTimeline::IsActive() {
  return document_ && document_->GetPage();
}

void DocumentTimeline::AnimationAttached(Animation& animation) {
  DCHECK_EQ(animation.TimelineInternal(), this);
  DCHECK(!animations_.Contains(&animation));
  animations_.insert(&animation);
}

Animation* DocumentTimeline::Play(AnimationEffectReadOnly* child) {
  if (!document_)
    return nullptr;

  Animation* animation = Animation::Create(child, this);
  DCHECK(animations_.Contains(animation));

  animation->play();
  DCHECK(animations_needing_update_.Contains(animation));

  return animation;
}

HeapVector<Member<Animation>> DocumentTimeline::getAnimations() {
  HeapVector<Member<Animation>> animations;
  for (const auto& animation : animations_) {
    if (animation->effect() &&
        (animation->effect()->IsCurrent() || animation->effect()->IsInEffect()))
      animations.push_back(animation);
  }
  std::sort(animations.begin(), animations.end(), CompareAnimations);
  return animations;
}

void DocumentTimeline::Wake() {
  timing_->ServiceOnNextFrame();
}

void DocumentTimeline::ServiceAnimations(TimingUpdateReason reason) {
  TRACE_EVENT0("blink", "DocumentTimeline::serviceAnimations");

  last_current_time_internal_ = CurrentTimeInternal();

  HeapVector<Member<Animation>> animations;
  animations.ReserveInitialCapacity(animations_needing_update_.size());
  for (Animation* animation : animations_needing_update_)
    animations.push_back(animation);

  std::sort(animations.begin(), animations.end(), Animation::HasLowerPriority);

  for (Animation* animation : animations) {
    if (!animation->Update(reason))
      animations_needing_update_.erase(animation);
  }

  DCHECK_EQ(outdated_animation_count_, 0U);
  DCHECK(last_current_time_internal_ == CurrentTimeInternal() ||
         (std::isnan(CurrentTimeInternal()) &&
          std::isnan(last_current_time_internal_)));

#if DCHECK_IS_ON()
  for (const auto& animation : animations_needing_update_)
    DCHECK(!animation->Outdated());
#endif
}

void DocumentTimeline::ScheduleNextService() {
  DCHECK_EQ(outdated_animation_count_, 0U);

  double time_to_next_effect = std::numeric_limits<double>::infinity();
  for (const auto& animation : animations_needing_update_) {
    time_to_next_effect =
        std::min(time_to_next_effect, animation->TimeToEffectChange());
  }

  if (time_to_next_effect < kMinimumDelay) {
    timing_->ServiceOnNextFrame();
  } else if (time_to_next_effect != std::numeric_limits<double>::infinity()) {
    timing_->WakeAfter(time_to_next_effect - kMinimumDelay);
  }
}

void DocumentTimeline::DocumentTimelineTiming::WakeAfter(double duration) {
  if (timer_.IsActive() && timer_.NextFireInterval() < duration)
    return;
  timer_.StartOneShot(duration, FROM_HERE);
}

void DocumentTimeline::DocumentTimelineTiming::ServiceOnNextFrame() {
  if (timeline_->document_ && timeline_->document_->View())
    timeline_->document_->View()->ScheduleAnimation();
}

void DocumentTimeline::DocumentTimelineTiming::Trace(blink::Visitor* visitor) {
  visitor->Trace(timeline_);
  DocumentTimeline::PlatformTiming::Trace(visitor);
}

size_t DocumentTimeline::MainThreadCompositableAnimationsCount() const {
  size_t main_thread_compositable_animations_count = 0;
  // TODO(crbug.com/781305): Restore the calculation here.
  return main_thread_compositable_animations_count;
}

double DocumentTimeline::ZeroTime() {
  if (!zero_time_initialized_ && document_ && document_->Loader()) {
    zero_time_ = document_->Loader()->GetTiming().ReferenceMonotonicTime() +
                 origin_time_;
    zero_time_initialized_ = true;
  }
  return zero_time_;
}

void DocumentTimeline::ResetForTesting() {
  zero_time_ = origin_time_;
  zero_time_initialized_ = true;
  playback_rate_ = 1;
  last_current_time_internal_ = 0;
}

double DocumentTimeline::currentTime(bool& is_null) {
  return CurrentTimeInternal(is_null) * 1000;
}

double DocumentTimeline::CurrentTimeInternal(bool& is_null) {
  if (!IsActive()) {
    is_null = true;
    return std::numeric_limits<double>::quiet_NaN();
  }
  double result =
      playback_rate_ == 0
          ? ZeroTime()
          : (GetDocument()->GetAnimationClock().CurrentTime() - ZeroTime()) *
                playback_rate_;
  is_null = std::isnan(result);
  return result;
}

double DocumentTimeline::currentTime() {
  return CurrentTimeInternal() * 1000;
}

double DocumentTimeline::CurrentTimeInternal() {
  bool is_null;
  return CurrentTimeInternal(is_null);
}

double DocumentTimeline::EffectiveTime() {
  double time = CurrentTimeInternal();
  return std::isnan(time) ? 0 : time;
}

void DocumentTimeline::PauseAnimationsForTesting(double pause_time) {
  for (const auto& animation : animations_needing_update_)
    animation->PauseForTesting(pause_time);
  ServiceAnimations(kTimingUpdateOnDemand);
}

bool DocumentTimeline::NeedsAnimationTimingUpdate() {
  if (CurrentTimeInternal() == last_current_time_internal_)
    return false;

  if (std::isnan(CurrentTimeInternal()) &&
      std::isnan(last_current_time_internal_))
    return false;

  // We allow m_lastCurrentTimeInternal to advance here when there
  // are no animations to allow animations spawned during style
  // recalc to not invalidate this flag.
  if (animations_needing_update_.IsEmpty())
    last_current_time_internal_ = CurrentTimeInternal();

  return !animations_needing_update_.IsEmpty();
}

void DocumentTimeline::ClearOutdatedAnimation(Animation* animation) {
  DCHECK(!animation->Outdated());
  outdated_animation_count_--;
}

void DocumentTimeline::SetOutdatedAnimation(Animation* animation) {
  DCHECK(animation->Outdated());
  outdated_animation_count_++;
  animations_needing_update_.insert(animation);
  if (IsActive() && !document_->GetPage()->Animator().IsServicingAnimations())
    timing_->ServiceOnNextFrame();
}

void DocumentTimeline::SetPlaybackRate(double playback_rate) {
  if (!IsActive())
    return;
  double current_time = CurrentTimeInternal();
  playback_rate_ = playback_rate;
  zero_time_ = playback_rate == 0
                   ? current_time
                   : GetDocument()->GetAnimationClock().CurrentTime() -
                         current_time / playback_rate;
  zero_time_initialized_ = true;

  // Corresponding compositor animation may need to be restarted to pick up
  // the new playback rate. Marking the effect changed forces this.
  SetAllCompositorPending(true);
}

void DocumentTimeline::SetAllCompositorPending(bool source_changed) {
  for (const auto& animation : animations_) {
    animation->SetCompositorPending(source_changed);
  }
}

double DocumentTimeline::PlaybackRate() const {
  return playback_rate_;
}

void DocumentTimeline::InvalidateKeyframeEffects(const TreeScope& tree_scope) {
  for (const auto& animation : animations_)
    animation->InvalidateKeyframeEffect(tree_scope);
}

void DocumentTimeline::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(timing_);
  visitor->Trace(animations_needing_update_);
  visitor->Trace(animations_);
  AnimationTimeline::Trace(visitor);
}

}  // namespace blink
