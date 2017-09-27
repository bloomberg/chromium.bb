// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CustomCompositorAnimations.h"

#include "core/animation/Animation.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/animatable/AnimatableDouble.h"
#include "core/animation/animatable/AnimatableTransform.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "core/dom/Document.h"
#include "platform/graphics/CompositorMutation.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/transforms/Matrix3DTransformOperation.h"
#include "platform/transforms/MatrixTransformOperation.h"
#include "platform/transforms/TransformOperations.h"

namespace blink {

namespace {

// Create keyframe effect with zero duration, fill mode forward, and two key
// frames with same value. This corresponding animation is always running and by
// updating the key frames we are able to control the applied value.
static KeyframeEffect* CreateInfiniteKeyFrameEffect(
    Element& element,
    CSSPropertyID property_id,
    RefPtr<AnimatableValue> value) {
  AnimatableValueKeyframeVector keyframes(2);
  keyframes[0] = AnimatableValueKeyframe::Create();
  keyframes[0]->SetOffset(0.0);
  keyframes[0]->SetPropertyValue(property_id, value.Get());
  keyframes[1] = AnimatableValueKeyframe::Create();
  keyframes[1]->SetOffset(1.0);
  keyframes[1]->SetPropertyValue(property_id, value.Get());
  keyframes[1]->SetComposite(EffectModel::kCompositeReplace);

  Timing timing;
  timing.iteration_duration = 0;
  timing.fill_mode = Timing::FillMode::FORWARDS;

  AnimatableValueKeyframeEffectModel* effect_model =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  return KeyframeEffect::Create(&element, effect_model, timing);
}

static KeyframeEffect* UpdateInfiniteKeyframeEffect(
    const KeyframeEffect& keyframe_effect,
    CSSPropertyID property_id,
    RefPtr<AnimatableValue> value) {
  const KeyframeVector& old_frames =
      ToAnimatableValueKeyframeEffectModel(keyframe_effect.Model())
          ->GetFrames();
  AnimatableValueKeyframeVector keyframes(2);
  keyframes[0] = ToAnimatableValueKeyframe(old_frames[0]->Clone().Get());
  keyframes[1] = ToAnimatableValueKeyframe(old_frames[1]->Clone().Get());
  keyframes[0]->SetPropertyValue(property_id, value.Get());
  keyframes[1]->SetPropertyValue(property_id, value.Get());

  AnimatableValueKeyframeEffectModel* effect_model =
      AnimatableValueKeyframeEffectModel::Create(keyframes);
  return KeyframeEffect::Create(keyframe_effect.Target(), effect_model,
                                keyframe_effect.SpecifiedTiming());
}

static Animation* CreateOrUpdateAnimation(Animation* animation,
                                          Element& element,
                                          CSSPropertyID property_id,
                                          RefPtr<AnimatableValue> new_value) {
  if (!animation) {
    KeyframeEffect* keyframe_effect = CreateInfiniteKeyFrameEffect(
        element, property_id, std::move(new_value));
    return element.GetDocument().Timeline().Play(keyframe_effect);
  }
  KeyframeEffect* keyframe_effect =
      UpdateInfiniteKeyframeEffect(*ToKeyframeEffect(animation->effect()),
                                   property_id, std::move(new_value));
  animation->setEffect(keyframe_effect);
  return animation;
}

}  // namespace

void CustomCompositorAnimations::ApplyUpdate(
    Element& element,
    const CompositorMutation& mutation) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc"),
               "CustomCompositorAnimations::applyUpdate");

  if (mutation.IsOpacityMutated()) {
    RefPtr<AnimatableValue> animatable_value =
        AnimatableDouble::Create(mutation.Opacity());
    animation_ = CreateOrUpdateAnimation(
        animation_, element, CSSPropertyOpacity, std::move(animatable_value));
  }
  if (mutation.IsTransformMutated()) {
    TransformOperations ops;
    ops.Operations().push_back(Matrix3DTransformOperation::Create(
        TransformationMatrix(mutation.Transform())));
    RefPtr<AnimatableValue> animatable_value =
        AnimatableTransform::Create(ops, 1);
    animation_ = CreateOrUpdateAnimation(
        animation_, element, CSSPropertyTransform, std::move(animatable_value));
  }
}

}  // namespace blink
