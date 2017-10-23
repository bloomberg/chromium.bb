// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/WorkletAnimation.h"

#include "core/animation/ElementAnimations.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/ScrollTimeline.h"
#include "core/animation/Timing.h"
#include "core/dom/Node.h"
#include "core/dom/NodeComputedStyle.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"

namespace blink {

namespace {
bool ValidateEffects(const HeapVector<Member<KeyframeEffectReadOnly>>& effects,
                     String& error_string) {
  if (effects.IsEmpty()) {
    error_string = "Effects array must be non-empty";
    return false;
  }

  Document& target_document = effects.at(0)->Target()->GetDocument();
  for (const auto& effect : effects) {
    if (effect->Target()->GetDocument() != target_document) {
      error_string = "All effects must target elements in the same document";
      return false;
    }
  }
  return true;
}

bool ValidateTimelines(HeapVector<DocumentTimelineOrScrollTimeline>& timelines,
                       String& error_string) {
  if (timelines.IsEmpty()) {
    error_string = "Timelines array must be non-empty";
    return false;
  }

  for (const auto& timeline : timelines) {
    if (timeline.IsScrollTimeline()) {
      DoubleOrScrollTimelineAutoKeyword time_range;
      timeline.GetAsScrollTimeline()->timeRange(time_range);
      if (time_range.IsScrollTimelineAutoKeyword()) {
        error_string = "ScrollTimeline timeRange must have non-auto value";
        return false;
      }
    }
  }
  return true;
}
}  // namespace

WorkletAnimation* WorkletAnimation::Create(
    String animator_name,
    const HeapVector<Member<KeyframeEffectReadOnly>>& effects,
    HeapVector<DocumentTimelineOrScrollTimeline>& timelines,
    scoped_refptr<SerializedScriptValue> options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  String error_string;
  if (!ValidateEffects(effects, error_string)) {
    exception_state.ThrowDOMException(kNotSupportedError, error_string);
    return nullptr;
  }

  if (!ValidateTimelines(timelines, error_string)) {
    exception_state.ThrowDOMException(kNotSupportedError, error_string);
    return nullptr;
  }

  Document& document = effects.at(0)->Target()->GetDocument();
  WorkletAnimation* animation = new WorkletAnimation(
      animator_name, document, effects, timelines, std::move(options));
  if (CompositorAnimationTimeline* compositor_timeline =
          document.Timeline().CompositorTimeline())
    compositor_timeline->PlayerAttached(*animation);

  return animation;
}

WorkletAnimation::WorkletAnimation(
    const String& animator_name,
    Document& document,
    const HeapVector<Member<KeyframeEffectReadOnly>>& effects,
    HeapVector<DocumentTimelineOrScrollTimeline>& timelines,
    scoped_refptr<SerializedScriptValue> options)
    : animator_name_(animator_name),
      play_state_(Animation::kIdle),
      document_(document),
      effects_(effects),
      timelines_(timelines),
      options_(std::move(options)) {
  DCHECK(IsMainThread());
  DCHECK(Platform::Current()->IsThreadedAnimationEnabled());
  DCHECK(Platform::Current()->CompositorSupport());
  compositor_player_ = CompositorAnimationPlayer::Create();
  compositor_player_->SetAnimationDelegate(this);
}

String WorkletAnimation::playState() {
  DCHECK(IsMainThread());
  return Animation::PlayStateString(play_state_);
}

void WorkletAnimation::play() {
  DCHECK(IsMainThread());
  if (play_state_ != Animation::kPending) {
    document_->GetWorkletAnimationController().AttachAnimation(*this);
    play_state_ = Animation::kPending;
  }
}

void WorkletAnimation::cancel() {
  DCHECK(IsMainThread());
  if (play_state_ != Animation::kIdle) {
    document_->GetWorkletAnimationController().DetachAnimation(*this);
    play_state_ = Animation::kIdle;
  }
}

bool WorkletAnimation::StartOnCompositor() {
  DCHECK(IsMainThread());
  // TODO(smcgruer): We need to start all of the effects, not just the first.
  double animation_playback_rate = 1;
  Element& target = *effects_.at(0)->Target();
  ToKeyframeEffectModelBase(effects_.at(0)->Model())
      ->SnapshotAllCompositorKeyframes(target, target.ComputedStyleRef(),
                                       target.ParentComputedStyle());
  bool success =
      effects_.at(0)
          ->CheckCanStartAnimationOnCompositor(animation_playback_rate)
          .Ok();
  // Currently |StartAnimationOnCompositor| is unable to propagate a
  // WorkletAnimation effect to the compositor, so this method is a no-op.
  // TODO(smcgruer): Actually create the element animations on the compositor.
  play_state_ = success ? Animation::kRunning : Animation::kIdle;
  return success;
}

void WorkletAnimation::Dispose() {
  DCHECK(IsMainThread());

  if (CompositorAnimationTimeline* compositor_timeline =
          document_->Timeline().CompositorTimeline())
    compositor_timeline->PlayerDestroyed(*this);
  compositor_player_->SetAnimationDelegate(nullptr);
  compositor_player_ = nullptr;
}

void WorkletAnimation::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(effects_);
  visitor->Trace(timelines_);
  WorkletAnimationBase::Trace(visitor);
}

}  // namespace blink
