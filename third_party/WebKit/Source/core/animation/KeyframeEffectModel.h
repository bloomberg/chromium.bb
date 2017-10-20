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

#ifndef KeyframeEffectModel_h
#define KeyframeEffectModel_h

#include <memory>
#include "core/CoreExport.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/EffectModel.h"
#include "core/animation/InterpolationEffect.h"
#include "core/animation/PropertyHandle.h"
#include "core/animation/StringKeyframe.h"
#include "core/animation/TransitionKeyframe.h"
#include "platform/animation/TimingFunction.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

class Element;
class KeyframeEffectModelTest;

class CORE_EXPORT KeyframeEffectModelBase : public EffectModel {
 public:
  // FIXME: Implement accumulation.

  using PropertySpecificKeyframeVector =
      Vector<scoped_refptr<Keyframe::PropertySpecificKeyframe>>;
  class PropertySpecificKeyframeGroup {
   public:
    void AppendKeyframe(scoped_refptr<Keyframe::PropertySpecificKeyframe>);
    const PropertySpecificKeyframeVector& Keyframes() const {
      return keyframes_;
    }

   private:
    void RemoveRedundantKeyframes();
    bool AddSyntheticKeyframeIfRequired(
        scoped_refptr<TimingFunction> zero_offset_easing);

    PropertySpecificKeyframeVector keyframes_;

    friend class KeyframeEffectModelBase;
  };

  bool IsReplaceOnly();

  PropertyHandleSet Properties() const;

  using KeyframeVector = Vector<scoped_refptr<Keyframe>>;
  const KeyframeVector& GetFrames() const { return keyframes_; }
  void SetFrames(KeyframeVector& keyframes);

  const PropertySpecificKeyframeVector& GetPropertySpecificKeyframes(
      const PropertyHandle& property) const {
    EnsureKeyframeGroups();
    return keyframe_groups_->at(property)->Keyframes();
  }

  using KeyframeGroupMap =
      HashMap<PropertyHandle, std::unique_ptr<PropertySpecificKeyframeGroup>>;
  const KeyframeGroupMap& GetPropertySpecificKeyframeGroups() const {
    EnsureKeyframeGroups();
    return *keyframe_groups_;
  }

  // EffectModel implementation.
  bool Sample(int iteration,
              double fraction,
              double iteration_duration,
              Vector<scoped_refptr<Interpolation>>&) const override;

  bool IsKeyframeEffectModel() const override { return true; }

  virtual bool IsStringKeyframeEffectModel() const { return false; }
  virtual bool IsTransitionKeyframeEffectModel() const { return false; }

  bool HasSyntheticKeyframes() const {
    EnsureKeyframeGroups();
    return has_synthetic_keyframes_;
  }

  bool NeedsCompositorKeyframesSnapshot() const {
    return needs_compositor_keyframes_snapshot_;
  }
  bool SnapshotNeutralCompositorKeyframes(
      Element&,
      const ComputedStyle& old_style,
      const ComputedStyle& new_style,
      const ComputedStyle* parent_style) const;
  bool SnapshotAllCompositorKeyframes(Element&,
                                      const ComputedStyle& base_style,
                                      const ComputedStyle* parent_style) const;

  static KeyframeVector NormalizedKeyframesForInspector(
      const KeyframeVector& keyframes) {
    return NormalizedKeyframes(keyframes);
  }

  bool Affects(const PropertyHandle& property) const override {
    EnsureKeyframeGroups();
    return keyframe_groups_->Contains(property);
  }

  bool IsTransformRelatedEffect() const override;

 protected:
  KeyframeEffectModelBase(scoped_refptr<TimingFunction> default_keyframe_easing)
      : last_iteration_(0),
        last_fraction_(std::numeric_limits<double>::quiet_NaN()),
        last_iteration_duration_(0),
        default_keyframe_easing_(std::move(default_keyframe_easing)),
        has_synthetic_keyframes_(false),
        needs_compositor_keyframes_snapshot_(true) {}

  static KeyframeVector NormalizedKeyframes(const KeyframeVector& keyframes);

  // Lazily computes the groups of property-specific keyframes.
  void EnsureKeyframeGroups() const;
  void EnsureInterpolationEffectPopulated() const;

  KeyframeVector keyframes_;
  // The spec describes filtering the normalized keyframes at sampling time
  // to get the 'property-specific keyframes'. For efficiency, we cache the
  // property-specific lists.
  mutable std::unique_ptr<KeyframeGroupMap> keyframe_groups_;
  mutable InterpolationEffect interpolation_effect_;
  mutable int last_iteration_;
  mutable double last_fraction_;
  mutable double last_iteration_duration_;
  scoped_refptr<TimingFunction> default_keyframe_easing_;

  mutable bool has_synthetic_keyframes_;
  mutable bool needs_compositor_keyframes_snapshot_;

  friend class KeyframeEffectModelTest;
};

// Time independent representation of an Animation's keyframes.
template <class Keyframe>
class KeyframeEffectModel final : public KeyframeEffectModelBase {
 public:
  using KeyframeVector = Vector<scoped_refptr<Keyframe>>;
  static KeyframeEffectModel<Keyframe>* Create(
      const KeyframeVector& keyframes,
      scoped_refptr<TimingFunction> default_keyframe_easing = nullptr) {
    return new KeyframeEffectModel(keyframes,
                                   std::move(default_keyframe_easing));
  }

 private:
  KeyframeEffectModel(const KeyframeVector& keyframes,
                      scoped_refptr<TimingFunction> default_keyframe_easing)
      : KeyframeEffectModelBase(std::move(default_keyframe_easing)) {
    keyframes_.AppendVector(keyframes);
  }

  virtual bool IsStringKeyframeEffectModel() const { return false; }
  virtual bool IsTransitionKeyframeEffectModel() const { return false; }
};

using KeyframeVector = KeyframeEffectModelBase::KeyframeVector;
using PropertySpecificKeyframeVector =
    KeyframeEffectModelBase::PropertySpecificKeyframeVector;

using StringKeyframeEffectModel = KeyframeEffectModel<StringKeyframe>;
using StringKeyframeVector = StringKeyframeEffectModel::KeyframeVector;
using StringPropertySpecificKeyframeVector =
    StringKeyframeEffectModel::PropertySpecificKeyframeVector;

using TransitionKeyframeEffectModel = KeyframeEffectModel<TransitionKeyframe>;
using TransitionKeyframeVector = TransitionKeyframeEffectModel::KeyframeVector;
using TransitionPropertySpecificKeyframeVector =
    TransitionKeyframeEffectModel::PropertySpecificKeyframeVector;

DEFINE_TYPE_CASTS(KeyframeEffectModelBase,
                  EffectModel,
                  value,
                  value->IsKeyframeEffectModel(),
                  value.IsKeyframeEffectModel());
DEFINE_TYPE_CASTS(StringKeyframeEffectModel,
                  KeyframeEffectModelBase,
                  value,
                  value->IsStringKeyframeEffectModel(),
                  value.IsStringKeyframeEffectModel());
DEFINE_TYPE_CASTS(TransitionKeyframeEffectModel,
                  KeyframeEffectModelBase,
                  value,
                  value->IsTransitionKeyframeEffectModel(),
                  value.IsTransitionKeyframeEffectModel());

inline const StringKeyframeEffectModel* ToStringKeyframeEffectModel(
    const EffectModel* base) {
  return ToStringKeyframeEffectModel(ToKeyframeEffectModelBase(base));
}

inline StringKeyframeEffectModel* ToStringKeyframeEffectModel(
    EffectModel* base) {
  return ToStringKeyframeEffectModel(ToKeyframeEffectModelBase(base));
}

inline TransitionKeyframeEffectModel* ToTransitionKeyframeEffectModel(
    EffectModel* base) {
  return ToTransitionKeyframeEffectModel(ToKeyframeEffectModelBase(base));
}

template <>
inline bool KeyframeEffectModel<StringKeyframe>::IsStringKeyframeEffectModel()
    const {
  return true;
}

template <>
inline bool KeyframeEffectModel<
    TransitionKeyframe>::IsTransitionKeyframeEffectModel() const {
  return true;
}

}  // namespace blink

#endif  // KeyframeEffectModel_h
