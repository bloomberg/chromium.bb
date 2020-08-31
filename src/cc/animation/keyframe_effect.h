// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_KEYFRAME_EFFECT_H_
#define CC_ANIMATION_KEYFRAME_EFFECT_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/element_animations.h"
#include "cc/paint/element_id.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/target_property.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/scroll_offset.h"

#include <memory>
#include <vector>

namespace cc {

class Animation;
struct PropertyAnimationState;

// A KeyframeEffect owns a group of KeyframeModels for a single target
// (identified by an ElementId). It is responsible for managing the
// KeyframeModels' running states (starting, running, paused, etc), as well as
// ticking the KeyframeModels when it is requested to produce new outputs for a
// given time.
//
// Note that a single KeyframeEffect may not own all the KeyframeModels for a
// given target. KeyframeEffect is only a grouping mechanism for related
// KeyframeModels. The commonality between keyframe models on the same target
// is found via ElementAnimations - there is only one ElementAnimations for a
// given target.
class CC_ANIMATION_EXPORT KeyframeEffect {
 public:
  explicit KeyframeEffect(Animation* animation);
  KeyframeEffect(const KeyframeEffect&) = delete;
  virtual ~KeyframeEffect();

  KeyframeEffect& operator=(const KeyframeEffect&) = delete;

  // ElementAnimations object where this controller is listed.
  scoped_refptr<ElementAnimations> element_animations() const {
    return element_animations_;
  }

  bool has_bound_element_animations() const { return !!element_animations_; }

  bool has_attached_element() const { return !!element_id_; }

  ElementId element_id() const { return element_id_; }

  // Returns true if there are any KeyframeModels at all to process.
  bool has_any_keyframe_model() const { return !keyframe_models_.empty(); }

  // When a scroll animation is removed on the main thread, its compositor
  // thread counterpart continues producing scroll deltas until activation.
  // These scroll deltas need to be cleared at activation, so that the active
  // element's scroll offset matches the offset provided by the main thread
  // rather than a combination of this offset and scroll deltas produced by the
  // removed animation. This is to provide the illusion of synchronicity to JS
  // that simultaneously removes an animation and sets the scroll offset.
  bool scroll_offset_animation_was_interrupted() const {
    return scroll_offset_animation_was_interrupted_;
  }

  bool needs_push_properties() const { return needs_push_properties_; }
  void SetNeedsPushProperties();

  void BindElementAnimations(ElementAnimations* element_animations);
  void UnbindElementAnimations();

  void AttachElement(ElementId element_id);
  void DetachElement();

  virtual void Tick(base::TimeTicks monotonic_time);
  static void TickKeyframeModel(base::TimeTicks monotonic_time,
                                KeyframeModel* keyframe_model,
                                AnimationTarget* target);
  void RemoveFromTicking();

  void UpdateState(bool start_ready_keyframe_models, AnimationEvents* events);
  void UpdateTickingState();

  void Pause(base::TimeDelta pause_offset);

  void AddKeyframeModel(std::unique_ptr<KeyframeModel> keyframe_model);
  void PauseKeyframeModel(int keyframe_model_id, base::TimeDelta time_offset);
  void RemoveKeyframeModel(int keyframe_model_id);
  void AbortKeyframeModel(int keyframe_model_id);
  void AbortKeyframeModelsWithProperty(TargetProperty::Type target_property,
                                       bool needs_completion);

  void ActivateKeyframeModels();

  void KeyframeModelAdded();

  // Dispatches animation event to a keyframe model specified as part of the
  // event. Returns true if the event is dispatched, false otherwise.
  bool DispatchAnimationEventToKeyframeModel(const AnimationEvent& event);

  // Returns true if there are any KeyframeModels that have neither finished
  // nor aborted.
  bool HasTickingKeyframeModel() const;
  size_t TickingKeyframeModelsCount() const;

  bool AffectsCustomProperty() const;

  bool HasNonDeletedKeyframeModel() const;

  bool AnimationsPreserveAxisAlignment() const;

  // Gets scales transform animations. On return, |maximum_scale| is the maximum
  // scale along any dimension at any destination in active scale animations,
  // and |starting_scale| is the maximum of starting animation scale along any
  // dimension at any destination in active scale animations. They are set to
  // kNotScaled if there is no active scale animation or the scales cannot be
  // computed. Returns false if the scales cannot be computed.
  bool GetAnimationScales(ElementListType,
                          float* maximum_scale,
                          float* starting_scale) const;

  // Returns true if there is a keyframe_model that is either currently
  // animating the given property or scheduled to animate this property in the
  // future, and that affects the given tree type.
  bool IsPotentiallyAnimatingProperty(TargetProperty::Type target_property,
                                      ElementListType list_type) const;

  // Returns true if there is a keyframe_model that is currently animating the
  // given property and that affects the given tree type.
  bool IsCurrentlyAnimatingProperty(TargetProperty::Type target_property,
                                    ElementListType list_type) const;

  KeyframeModel* GetKeyframeModel(TargetProperty::Type target_property) const;
  KeyframeModel* GetKeyframeModelById(int keyframe_model_id) const;

  void GetPropertyAnimationState(PropertyAnimationState* pending_state,
                                 PropertyAnimationState* active_state) const;

  void MarkAbortedKeyframeModelsForDeletion(
      KeyframeEffect* element_keyframe_effect_impl);
  void PurgeKeyframeModelsMarkedForDeletion(bool impl_only);
  void PushNewKeyframeModelsToImplThread(
      KeyframeEffect* element_keyframe_effect_impl) const;
  void RemoveKeyframeModelsCompletedOnMainThread(
      KeyframeEffect* element_keyframe_effect_impl) const;
  void PushPropertiesTo(KeyframeEffect* keyframe_effect_impl);

  std::string KeyframeModelsToString() const;

 private:
  void StartKeyframeModels(base::TimeTicks monotonic_time);
  void PromoteStartedKeyframeModels(AnimationEvents* events);
  void PurgeDeletedKeyframeModels();

  void MarkKeyframeModelsForDeletion(base::TimeTicks, AnimationEvents* events);
  void MarkFinishedKeyframeModels(base::TimeTicks monotonic_time);

  bool HasElementInActiveList() const;
  gfx::ScrollOffset ScrollOffsetForAnimation() const;
  void GenerateEvent(AnimationEvents* events,
                     const KeyframeModel& keyframe_model,
                     AnimationEvent::Type type,
                     base::TimeTicks monotonic_time);
  void GenerateTakeoverEventForScrollAnimation(
      AnimationEvents* events,
      const KeyframeModel& keyframe_model,
      base::TimeTicks monotonic_time);

  std::vector<std::unique_ptr<KeyframeModel>> keyframe_models_;
  Animation* animation_;

  ElementId element_id_;

  // element_animations_ is non-null if controller is attached to an element.
  scoped_refptr<ElementAnimations> element_animations_;

  // Only try to start KeyframeModels when new keyframe models are added or
  // when the previous attempt at starting KeyframeModels failed to start all
  // KeyframeModels.
  bool needs_to_start_keyframe_models_;

  bool scroll_offset_animation_was_interrupted_;

  bool is_ticking_;
  base::Optional<base::TimeTicks> last_tick_time_;

  bool needs_push_properties_;
};

}  // namespace cc

#endif  // CC_ANIMATION_KEYFRAME_EFFECT_H_
