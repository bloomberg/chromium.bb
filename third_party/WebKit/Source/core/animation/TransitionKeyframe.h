// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransitionKeyframe_h
#define TransitionKeyframe_h

#include "core/CoreExport.h"
#include "core/animation/Keyframe.h"
#include "core/animation/TypedInterpolationValue.h"
#include "core/animation/animatable/AnimatableValue.h"

namespace blink {

class CORE_EXPORT TransitionKeyframe : public Keyframe {
 public:
  static PassRefPtr<TransitionKeyframe> create(const PropertyHandle& property) {
    return adoptRef(new TransitionKeyframe(property));
  }
  void setValue(std::unique_ptr<TypedInterpolationValue> value) {
    m_value = std::move(value);
  }
  void setCompositorValue(RefPtr<AnimatableValue>);
  PropertyHandleSet properties() const final;

  class PropertySpecificKeyframe : public Keyframe::PropertySpecificKeyframe {
   public:
    static PassRefPtr<PropertySpecificKeyframe> create(
        double offset,
        PassRefPtr<TimingFunction> easing,
        EffectModel::CompositeOperation composite,
        std::unique_ptr<TypedInterpolationValue> value,
        RefPtr<AnimatableValue> compositorValue) {
      return adoptRef(new PropertySpecificKeyframe(offset, std::move(easing),
                                                   composite, std::move(value),
                                                   std::move(compositorValue)));
    }

    PassRefPtr<AnimatableValue> getAnimatableValue() const final {
      return m_compositorValue;
    }

    bool isNeutral() const final { return false; }
    PassRefPtr<Keyframe::PropertySpecificKeyframe> neutralKeyframe(
        double offset,
        PassRefPtr<TimingFunction> easing) const final {
      NOTREACHED();
      return nullptr;
    }
    PassRefPtr<Interpolation> createInterpolation(
        const PropertyHandle&,
        const Keyframe::PropertySpecificKeyframe& other) const final;

    bool isTransitionPropertySpecificKeyframe() const final { return true; }

   private:
    PropertySpecificKeyframe(double offset,
                             PassRefPtr<TimingFunction> easing,
                             EffectModel::CompositeOperation composite,
                             std::unique_ptr<TypedInterpolationValue> value,
                             RefPtr<AnimatableValue> compositorValue)
        : Keyframe::PropertySpecificKeyframe(offset,
                                             std::move(easing),
                                             composite),
          m_value(std::move(value)),
          m_compositorValue(std::move(compositorValue)) {}

    PassRefPtr<Keyframe::PropertySpecificKeyframe> cloneWithOffset(
        double offset) const final {
      return create(offset, m_easing, m_composite, m_value->clone(),
                    m_compositorValue);
    }

    std::unique_ptr<TypedInterpolationValue> m_value;
    RefPtr<AnimatableValue> m_compositorValue;
  };

 private:
  TransitionKeyframe(const PropertyHandle& property) : m_property(property) {}

  TransitionKeyframe(const TransitionKeyframe& copyFrom)
      : Keyframe(copyFrom.m_offset, copyFrom.m_composite, copyFrom.m_easing),
        m_property(copyFrom.m_property),
        m_value(copyFrom.m_value->clone()),
        m_compositorValue(copyFrom.m_compositorValue) {}

  bool isTransitionKeyframe() const final { return true; }

  PassRefPtr<Keyframe> clone() const final {
    return adoptRef(new TransitionKeyframe(*this));
  }

  PassRefPtr<Keyframe::PropertySpecificKeyframe> createPropertySpecificKeyframe(
      const PropertyHandle&) const final;

  PropertyHandle m_property;
  std::unique_ptr<TypedInterpolationValue> m_value;
  RefPtr<AnimatableValue> m_compositorValue;
};

using TransitionPropertySpecificKeyframe =
    TransitionKeyframe::PropertySpecificKeyframe;

DEFINE_TYPE_CASTS(TransitionKeyframe,
                  Keyframe,
                  value,
                  value->isTransitionKeyframe(),
                  value.isTransitionKeyframe());
DEFINE_TYPE_CASTS(TransitionPropertySpecificKeyframe,
                  Keyframe::PropertySpecificKeyframe,
                  value,
                  value->isTransitionPropertySpecificKeyframe(),
                  value.isTransitionPropertySpecificKeyframe());

}  // namespace blink

#endif
