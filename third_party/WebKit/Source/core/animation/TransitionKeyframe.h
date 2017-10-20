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
  static scoped_refptr<TransitionKeyframe> Create(
      const PropertyHandle& property) {
    return WTF::AdoptRef(new TransitionKeyframe(property));
  }
  void SetValue(std::unique_ptr<TypedInterpolationValue> value) {
    value_ = std::move(value);
  }
  void SetCompositorValue(scoped_refptr<AnimatableValue>);
  PropertyHandleSet Properties() const final;

  class PropertySpecificKeyframe : public Keyframe::PropertySpecificKeyframe {
   public:
    static scoped_refptr<PropertySpecificKeyframe> Create(
        double offset,
        scoped_refptr<TimingFunction> easing,
        EffectModel::CompositeOperation composite,
        std::unique_ptr<TypedInterpolationValue> value,
        scoped_refptr<AnimatableValue> compositor_value) {
      return WTF::AdoptRef(new PropertySpecificKeyframe(
          offset, std::move(easing), composite, std::move(value),
          std::move(compositor_value)));
    }

    const AnimatableValue* GetAnimatableValue() const final {
      return compositor_value_.get();
    }

    bool IsNeutral() const final { return false; }
    scoped_refptr<Keyframe::PropertySpecificKeyframe> NeutralKeyframe(
        double offset,
        scoped_refptr<TimingFunction> easing) const final {
      NOTREACHED();
      return nullptr;
    }
    scoped_refptr<Interpolation> CreateInterpolation(
        const PropertyHandle&,
        const Keyframe::PropertySpecificKeyframe& other) const final;

    bool IsTransitionPropertySpecificKeyframe() const final { return true; }

   private:
    PropertySpecificKeyframe(double offset,
                             scoped_refptr<TimingFunction> easing,
                             EffectModel::CompositeOperation composite,
                             std::unique_ptr<TypedInterpolationValue> value,
                             scoped_refptr<AnimatableValue> compositor_value)
        : Keyframe::PropertySpecificKeyframe(offset,
                                             std::move(easing),
                                             composite),
          value_(std::move(value)),
          compositor_value_(std::move(compositor_value)) {}

    scoped_refptr<Keyframe::PropertySpecificKeyframe> CloneWithOffset(
        double offset) const final {
      return Create(offset, easing_, composite_, value_->Clone(),
                    compositor_value_);
    }

    std::unique_ptr<TypedInterpolationValue> value_;
    scoped_refptr<AnimatableValue> compositor_value_;
  };

 private:
  TransitionKeyframe(const PropertyHandle& property) : property_(property) {}

  TransitionKeyframe(const TransitionKeyframe& copy_from)
      : Keyframe(copy_from.offset_, copy_from.composite_, copy_from.easing_),
        property_(copy_from.property_),
        value_(copy_from.value_->Clone()),
        compositor_value_(copy_from.compositor_value_) {}

  bool IsTransitionKeyframe() const final { return true; }

  scoped_refptr<Keyframe> Clone() const final {
    return WTF::AdoptRef(new TransitionKeyframe(*this));
  }

  scoped_refptr<Keyframe::PropertySpecificKeyframe>
  CreatePropertySpecificKeyframe(const PropertyHandle&) const final;

  PropertyHandle property_;
  std::unique_ptr<TypedInterpolationValue> value_;
  scoped_refptr<AnimatableValue> compositor_value_;
};

using TransitionPropertySpecificKeyframe =
    TransitionKeyframe::PropertySpecificKeyframe;

DEFINE_TYPE_CASTS(TransitionKeyframe,
                  Keyframe,
                  value,
                  value->IsTransitionKeyframe(),
                  value.IsTransitionKeyframe());
DEFINE_TYPE_CASTS(TransitionPropertySpecificKeyframe,
                  Keyframe::PropertySpecificKeyframe,
                  value,
                  value->IsTransitionPropertySpecificKeyframe(),
                  value.IsTransitionPropertySpecificKeyframe());

}  // namespace blink

#endif
