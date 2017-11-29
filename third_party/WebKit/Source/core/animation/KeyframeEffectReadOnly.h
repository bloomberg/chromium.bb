// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KeyframeEffectReadOnly_h
#define KeyframeEffectReadOnly_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/CompositorAnimations.h"
#include "core/animation/KeyframeEffectModel.h"

namespace blink {

class DictionarySequenceOrDictionary;
class Element;
class ExceptionState;
class ExecutionContext;
class PropertyHandle;
class SampledEffect;
class ScriptState;
class UnrestrictedDoubleOrKeyframeEffectOptions;

// Represents the effect of an Animation on an Element's properties.
// http://w3c.github.io/web-animations/#the-keyframeeffect-interfaces
class CORE_EXPORT KeyframeEffectReadOnly : public AnimationEffectReadOnly {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Priority { kDefaultPriority, kTransitionPriority };

  static KeyframeEffectReadOnly* Create(Element*,
                                        KeyframeEffectModelBase*,
                                        const Timing&,
                                        Priority = kDefaultPriority,
                                        EventDelegate* = nullptr);
  // Web Animations API Bindings constructors.
  static KeyframeEffectReadOnly* Create(
      ExecutionContext*,
      Element*,
      const DictionarySequenceOrDictionary& effect_input,
      const UnrestrictedDoubleOrKeyframeEffectOptions&,
      ExceptionState&);
  static KeyframeEffectReadOnly* Create(
      ExecutionContext*,
      Element*,
      const DictionarySequenceOrDictionary& effect_input,
      ExceptionState&);

  ~KeyframeEffectReadOnly() override {}

  bool IsKeyframeEffectReadOnly() const override { return true; }

  // IDL implementation.
  String composite() const;
  Vector<ScriptValue> getKeyframes(ScriptState*);

  EffectModel::CompositeOperation compositeInternal() const {
    return model_->Composite();
  }

  bool Affects(const PropertyHandle&) const;
  const KeyframeEffectModelBase* Model() const { return model_.Get(); }
  KeyframeEffectModelBase* Model() { return model_.Get(); }
  void SetModel(KeyframeEffectModelBase* model) {
    DCHECK(model);
    model_ = model;
  }
  Priority GetPriority() const { return priority_; }
  Element* Target() const { return target_; }

  void NotifySampledEffectRemovedFromEffectStack();

  CompositorAnimations::FailureCode CheckCanStartAnimationOnCompositor(
      double animation_playback_rate) const;
  // Must only be called once.
  void StartAnimationOnCompositor(
      int group,
      double start_time,
      double time_offset,
      double animation_playback_rate,
      CompositorAnimationPlayer* compositor_player = nullptr);
  bool HasActiveAnimationsOnCompositor() const;
  bool HasActiveAnimationsOnCompositor(const PropertyHandle&) const;
  bool CancelAnimationOnCompositor();
  void RestartAnimationOnCompositor();
  void CancelIncompatibleAnimationsOnCompositor();
  void PauseAnimationForTestingOnCompositor(double pause_time);

  void AttachCompositedLayers();

  void SetCompositorAnimationIdsForTesting(
      const Vector<int>& compositor_animation_ids) {
    compositor_animation_ids_ = compositor_animation_ids;
  }

  void Trace(blink::Visitor*) override;

  void DowngradeToNormal() { priority_ = kDefaultPriority; }

 protected:
  KeyframeEffectReadOnly(Element*,
                         KeyframeEffectModelBase*,
                         const Timing&,
                         Priority,
                         EventDelegate*);

  void ApplyEffects();
  void ClearEffects();
  void UpdateChildrenAndEffects() const override;
  void Attach(Animation*) override;
  void Detach() override;
  void SpecifiedTimingChanged() override;
  double CalculateTimeToEffectChange(
      bool forwards,
      double inherited_time,
      double time_to_next_iteration) const override;
  virtual bool HasIncompatibleStyle();
  bool HasMultipleTransformProperties() const;

 private:
  Member<Element> target_;
  Member<KeyframeEffectModelBase> model_;
  Member<SampledEffect> sampled_effect_;

  Priority priority_;

  Vector<int> compositor_animation_ids_;
};

DEFINE_TYPE_CASTS(KeyframeEffectReadOnly,
                  AnimationEffectReadOnly,
                  animationNode,
                  animationNode->IsKeyframeEffectReadOnly(),
                  animationNode.IsKeyframeEffectReadOnly());

}  // namespace blink

#endif  // KeyframeEffectReadOnly_h
