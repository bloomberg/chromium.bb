// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Keyframe_h
#define Keyframe_h

#include "core/CoreExport.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/EffectModel.h"
#include "core/animation/PropertyHandle.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

using PropertyHandleSet = HashSet<PropertyHandle>;

class Element;
class ComputedStyle;

// Represents a user specificed keyframe in a KeyframeEffect.
// http://w3c.github.io/web-animations/#keyframe
// FIXME: Make Keyframe immutable
class CORE_EXPORT Keyframe : public RefCounted<Keyframe> {
  USING_FAST_MALLOC(Keyframe);
  WTF_MAKE_NONCOPYABLE(Keyframe);

 public:
  virtual ~Keyframe() {}

  void SetOffset(double offset) { offset_ = offset; }
  double Offset() const { return offset_; }

  void SetComposite(EffectModel::CompositeOperation composite) {
    composite_ = composite;
  }
  EffectModel::CompositeOperation Composite() const { return composite_; }

  void SetEasing(PassRefPtr<TimingFunction> easing) {
    easing_ = std::move(easing);
  }
  TimingFunction& Easing() const { return *easing_; }

  static bool CompareOffsets(const RefPtr<Keyframe>& a,
                             const RefPtr<Keyframe>& b) {
    return a->Offset() < b->Offset();
  }

  virtual PropertyHandleSet Properties() const = 0;

  virtual PassRefPtr<Keyframe> Clone() const = 0;
  PassRefPtr<Keyframe> CloneWithOffset(double offset) const {
    RefPtr<Keyframe> the_clone = Clone();
    the_clone->SetOffset(offset);
    return the_clone;
  }

  virtual bool IsAnimatableValueKeyframe() const { return false; }
  virtual bool IsStringKeyframe() const { return false; }
  virtual bool IsTransitionKeyframe() const { return false; }

  // Represents a property-value pair in a keyframe.
  class PropertySpecificKeyframe : public RefCounted<PropertySpecificKeyframe> {
    USING_FAST_MALLOC(PropertySpecificKeyframe);
    WTF_MAKE_NONCOPYABLE(PropertySpecificKeyframe);

   public:
    virtual ~PropertySpecificKeyframe() {}
    double Offset() const { return offset_; }
    TimingFunction& Easing() const { return *easing_; }
    EffectModel::CompositeOperation Composite() const { return composite_; }
    double UnderlyingFraction() const {
      return composite_ == EffectModel::kCompositeReplace ? 0 : 1;
    }
    virtual bool IsNeutral() const = 0;
    virtual PassRefPtr<PropertySpecificKeyframe> CloneWithOffset(
        double offset) const = 0;

    // FIXME: Remove this once CompositorAnimations no longer depends on
    // AnimatableValues
    virtual bool PopulateAnimatableValue(
        CSSPropertyID,
        Element&,
        const ComputedStyle& base_style,
        const ComputedStyle* parent_style) const {
      return false;
    }
    virtual const AnimatableValue* GetAnimatableValue() const = 0;

    virtual bool IsAnimatableValuePropertySpecificKeyframe() const {
      return false;
    }
    virtual bool IsCSSPropertySpecificKeyframe() const { return false; }
    virtual bool IsSVGPropertySpecificKeyframe() const { return false; }
    virtual bool IsTransitionPropertySpecificKeyframe() const { return false; }

    virtual PassRefPtr<PropertySpecificKeyframe> NeutralKeyframe(
        double offset,
        PassRefPtr<TimingFunction> easing) const = 0;
    virtual PassRefPtr<Interpolation> CreateInterpolation(
        const PropertyHandle&,
        const Keyframe::PropertySpecificKeyframe& end) const;

   protected:
    PropertySpecificKeyframe(double offset,
                             PassRefPtr<TimingFunction> easing,
                             EffectModel::CompositeOperation);

    double offset_;
    RefPtr<TimingFunction> easing_;
    EffectModel::CompositeOperation composite_;
  };

  virtual PassRefPtr<PropertySpecificKeyframe> CreatePropertySpecificKeyframe(
      const PropertyHandle&) const = 0;

 protected:
  Keyframe()
      : offset_(NullValue()),
        composite_(EffectModel::kCompositeReplace),
        easing_(LinearTimingFunction::Shared()) {}
  Keyframe(double offset,
           EffectModel::CompositeOperation composite,
           PassRefPtr<TimingFunction> easing)
      : offset_(offset), composite_(composite), easing_(std::move(easing)) {}

  double offset_;
  EffectModel::CompositeOperation composite_;
  RefPtr<TimingFunction> easing_;
};

using PropertySpecificKeyframe = Keyframe::PropertySpecificKeyframe;

}  // namespace blink

#endif  // Keyframe_h
