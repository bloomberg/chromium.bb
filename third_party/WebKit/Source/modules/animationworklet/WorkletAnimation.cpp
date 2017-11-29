// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/animationworklet/WorkletAnimation.h"

#include "bindings/modules/v8/animation_effect_read_only_or_animation_effect_read_only_sequence.h"
#include "core/animation/ElementAnimations.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/ScrollTimeline.h"
#include "core/animation/Timing.h"
#include "core/dom/Node.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/layout/LayoutBox.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"

namespace blink {

namespace {
bool ConvertAnimationEffects(
    const AnimationEffectReadOnlyOrAnimationEffectReadOnlySequence& effects,
    HeapVector<Member<KeyframeEffectReadOnly>>& keyframe_effects,
    String& error_string) {
  DCHECK(keyframe_effects.IsEmpty());

  // Currently we only support KeyframeEffectReadOnly (and its subclasses).
  if (effects.IsAnimationEffectReadOnly()) {
    const auto& effect = effects.GetAsAnimationEffectReadOnly();
    if (!effect->IsKeyframeEffectReadOnly()) {
      error_string = "Effect must be a KeyframeEffectReadOnly object";
      return false;
    }
    keyframe_effects.push_back(ToKeyframeEffectReadOnly(effect));
  } else {
    const HeapVector<Member<AnimationEffectReadOnly>>& effect_sequence =
        effects.GetAsAnimationEffectReadOnlySequence();
    keyframe_effects.ReserveInitialCapacity(effect_sequence.size());
    for (const auto& effect : effect_sequence) {
      if (!effect->IsKeyframeEffectReadOnly()) {
        error_string = "Effects must all be KeyframeEffectReadOnly objects";
        return false;
      }
      keyframe_effects.push_back(ToKeyframeEffectReadOnly(effect));
    }
  }

  if (keyframe_effects.IsEmpty()) {
    error_string = "Effects array must be non-empty";
    return false;
  }

  // TODO(crbug.com/781816): Allow using effects with no target.
  for (const auto& effect : keyframe_effects) {
    if (!effect->Target()) {
      error_string = "All effect targets must exist";
      return false;
    }
  }

  Document& target_document = keyframe_effects.at(0)->Target()->GetDocument();
  for (const auto& effect : keyframe_effects) {
    if (effect->Target()->GetDocument() != target_document) {
      error_string = "All effects must target elements in the same document";
      return false;
    }
  }
  return true;
}

bool ValidateTimeline(const DocumentTimelineOrScrollTimeline& timeline,
                      String& error_string) {
  if (timeline.IsScrollTimeline()) {
    DoubleOrScrollTimelineAutoKeyword time_range;
    timeline.GetAsScrollTimeline()->timeRange(time_range);
    if (time_range.IsScrollTimelineAutoKeyword()) {
      error_string = "ScrollTimeline timeRange must have non-auto value";
      return false;
    }
  }
  return true;
}

bool CheckElementComposited(const Element& target) {
  return target.GetLayoutObject() &&
         target.GetLayoutObject()->GetCompositingState() ==
             kPaintsIntoOwnBacking;
}

CompositorElementId GetCompositorScrollElementId(const Element& element) {
  DCHECK(element.GetLayoutObject());
  DCHECK(element.GetLayoutObject()->HasLayer());
  return CompositorElementIdFromUniqueObjectId(
      element.GetLayoutObject()->UniqueId(),
      CompositorElementIdNamespace::kScroll);
}

// Convert the blink concept of a ScrollTimeline orientation into the cc one.
//
// The compositor does not know about writing modes, so we have to convert the
// web concepts of 'block' and 'inline' direction into absolute vertical or
// horizontal directions.
//
// TODO(smcgruer): If the writing mode of a scroller changes, we have to update
// any related cc::ScrollTimeline somehow.
CompositorScrollTimeline::ScrollDirection ConvertOrientation(
    ScrollTimeline::ScrollDirection orientation,
    bool is_horizontal_writing_mode) {
  switch (orientation) {
    case ScrollTimeline::Block:
      return is_horizontal_writing_mode ? CompositorScrollTimeline::Vertical
                                        : CompositorScrollTimeline::Horizontal;
    case ScrollTimeline::Inline:
      return is_horizontal_writing_mode ? CompositorScrollTimeline::Horizontal
                                        : CompositorScrollTimeline::Vertical;
    default:
      NOTREACHED();
      return CompositorScrollTimeline::Vertical;
  }
}

// Converts a blink::ScrollTimeline into a cc::ScrollTimeline.
//
// If the timeline cannot be converted, returns nullptr.
std::unique_ptr<CompositorScrollTimeline> ToCompositorScrollTimeline(
    const DocumentTimelineOrScrollTimeline& timeline) {
  if (!timeline.IsScrollTimeline())
    return nullptr;

  ScrollTimeline* scroll_timeline = timeline.GetAsScrollTimeline();
  Element* scroll_source = scroll_timeline->scrollSource();
  CompositorElementId element_id = GetCompositorScrollElementId(*scroll_source);

  DoubleOrScrollTimelineAutoKeyword time_range;
  scroll_timeline->timeRange(time_range);
  // TODO(smcgruer): Handle 'auto' time range value.
  DCHECK(time_range.IsDouble());

  LayoutBox* box = scroll_source->GetLayoutBox();
  DCHECK(box);
  CompositorScrollTimeline::ScrollDirection orientation = ConvertOrientation(
      scroll_timeline->GetOrientation(), box->IsHorizontalWritingMode());

  return std::make_unique<CompositorScrollTimeline>(element_id, orientation,
                                                    time_range.GetAsDouble());
}
}  // namespace

WorkletAnimation* WorkletAnimation::Create(
    String animator_name,
    const AnimationEffectReadOnlyOrAnimationEffectReadOnlySequence& effects,
    DocumentTimelineOrScrollTimeline timeline,
    scoped_refptr<SerializedScriptValue> options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  HeapVector<Member<KeyframeEffectReadOnly>> keyframe_effects;
  String error_string;
  if (!ConvertAnimationEffects(effects, keyframe_effects, error_string)) {
    exception_state.ThrowDOMException(kNotSupportedError, error_string);
    return nullptr;
  }

  if (!ValidateTimeline(timeline, error_string)) {
    exception_state.ThrowDOMException(kNotSupportedError, error_string);
    return nullptr;
  }

  Document& document = keyframe_effects.at(0)->Target()->GetDocument();
  WorkletAnimation* animation = new WorkletAnimation(
      animator_name, document, keyframe_effects, timeline, std::move(options));

  return animation;
}

WorkletAnimation::WorkletAnimation(
    const String& animator_name,
    Document& document,
    const HeapVector<Member<KeyframeEffectReadOnly>>& effects,
    DocumentTimelineOrScrollTimeline timeline,
    scoped_refptr<SerializedScriptValue> options)
    : animator_name_(animator_name),
      play_state_(Animation::kIdle),
      document_(document),
      effects_(effects),
      timeline_(timeline),
      options_(std::move(options)) {
  DCHECK(IsMainThread());
  DCHECK(Platform::Current()->IsThreadedAnimationEnabled());
  DCHECK(Platform::Current()->CompositorSupport());
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

bool WorkletAnimation::StartOnCompositor(String* failure_message) {
  DCHECK(IsMainThread());
  KeyframeEffectReadOnly* target_effect = effects_.at(0);
  Element& target = *target_effect->Target();

  // CheckCanStartAnimationOnCompositor requires that the property-specific
  // keyframe groups have been created. To ensure this we manually snapshot the
  // frames in the target effect.
  // TODO(smcgruer): This shouldn't be necessary - Animation doesn't do this.
  target_effect->Model()->SnapshotAllCompositorKeyframes(
      target, target.ComputedStyleRef(), target.ParentComputedStyle());

  if (!CheckElementComposited(target)) {
    if (failure_message)
      *failure_message = "The target element is not composited.";
    return false;
  }

  if (timeline_.IsScrollTimeline() &&
      !CheckElementComposited(
          *timeline_.GetAsScrollTimeline()->scrollSource())) {
    if (failure_message)
      *failure_message = "The ScrollTimeline scrollSource is not composited.";
    return false;
  }

  double playback_rate = 1;
  CompositorAnimations::FailureCode failure_code =
      target_effect->CheckCanStartAnimationOnCompositor(playback_rate);

  if (!failure_code.Ok()) {
    play_state_ = Animation::kIdle;
    if (failure_message)
      *failure_message = failure_code.reason;
    return false;
  }

  if (!compositor_player_) {
    compositor_player_ = CompositorAnimationPlayer::CreateWorkletPlayer(
        animator_name_, ToCompositorScrollTimeline(timeline_));
    compositor_player_->SetAnimationDelegate(this);
  }

  // Register ourselves on the compositor timeline. This will cause our cc-side
  // animation player to be registered.
  if (CompositorAnimationTimeline* compositor_timeline =
          document_->Timeline().CompositorTimeline())
    compositor_timeline->PlayerAttached(*this);

  // TODO(smcgruer): Creating a WorkletAnimation should be a hint to blink
  // compositing that the animated element should be promoted. Otherwise this
  // fails. http://crbug.com/776533
  CompositorAnimations::AttachCompositedLayers(target,
                                               compositor_player_.get());

  double start_time = std::numeric_limits<double>::quiet_NaN();
  double time_offset = 0;
  int group = 0;

  // TODO(smcgruer): We need to start all of the effects, not just the first.
  effects_.at(0)->StartAnimationOnCompositor(
      group, start_time, time_offset, playback_rate, compositor_player_.get());
  play_state_ = Animation::kRunning;
  return true;
}

void WorkletAnimation::Dispose() {
  DCHECK(IsMainThread());

  if (CompositorAnimationTimeline* compositor_timeline =
          document_->Timeline().CompositorTimeline())
    compositor_timeline->PlayerDestroyed(*this);
  if (compositor_player_) {
    compositor_player_->SetAnimationDelegate(nullptr);
    compositor_player_ = nullptr;
  }
}

void WorkletAnimation::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(effects_);
  visitor->Trace(timeline_);
  WorkletAnimationBase::Trace(visitor);
}

}  // namespace blink
