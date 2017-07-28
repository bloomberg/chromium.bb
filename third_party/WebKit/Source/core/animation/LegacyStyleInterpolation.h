// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LegacyStyleInterpolation_h
#define LegacyStyleInterpolation_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/animation/Interpolation.h"
#include "core/animation/PropertyHandle.h"
#include <memory>

namespace blink {

class StyleResolverState;

// See the documentation of Interpolation for general information about this
// class hierarchy.
//
// The LegacyStyleInterpolation subclass stores the start and end keyframes as
// InterpolableAnimatableValue objects. As the name implies, this class is
// legacy code and should not be used in new code.
// TODO(crbug.com/442163): Delete this class once no interpolation code depends
// on AnimatableValues.
//
// During the effect application phase of animation computation, the current
// value of the property is applied to the element by calling the Apply
// function.
class CORE_EXPORT LegacyStyleInterpolation : public Interpolation {
 public:
  static PassRefPtr<LegacyStyleInterpolation> Create(
      PassRefPtr<AnimatableValue> start,
      PassRefPtr<AnimatableValue> end,
      CSSPropertyID id) {
    return AdoptRef(new LegacyStyleInterpolation(
        InterpolableAnimatableValue::Create(std::move(start)),
        InterpolableAnimatableValue::Create(std::move(end)), id));
  }

  void Apply(StyleResolverState&) const;

  bool IsLegacyStyleInterpolation() const final { return true; }

  PassRefPtr<AnimatableValue> CurrentValue() const {
    return ToInterpolableAnimatableValue(cached_value_.get())->Value();
  }

  CSSPropertyID Id() const { return property_.CssProperty(); }

  const PropertyHandle& GetProperty() const final { return property_; }

  void Interpolate(int iteration, double fraction) final;

 protected:
  LegacyStyleInterpolation(std::unique_ptr<InterpolableValue> start,
                           std::unique_ptr<InterpolableValue> end,
                           CSSPropertyID);

 private:
  const std::unique_ptr<InterpolableValue> start_;
  const std::unique_ptr<InterpolableValue> end_;
  PropertyHandle property_;

  mutable double cached_fraction_;
  mutable int cached_iteration_;
  mutable std::unique_ptr<InterpolableValue> cached_value_;

  InterpolableValue* GetCachedValueForTesting() const {
    return cached_value_.get();
  }

  friend class AnimationInterpolableValueTest;
  friend class AnimationInterpolationEffectTest;
};

DEFINE_TYPE_CASTS(LegacyStyleInterpolation,
                  Interpolation,
                  value,
                  value->IsLegacyStyleInterpolation(),
                  value.IsLegacyStyleInterpolation());

}  // namespace blink

#endif
