// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LegacyStyleInterpolation_h
#define LegacyStyleInterpolation_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/animation/Interpolation.h"
#include "core/animation/PropertyHandle.h"
#include "core/css/resolver/AnimatedStyleBuilder.h"
#include <memory>

namespace blink {

class StyleResolverState;

class CORE_EXPORT LegacyStyleInterpolation : public Interpolation {
 public:
  static PassRefPtr<LegacyStyleInterpolation> create(
      PassRefPtr<AnimatableValue> start,
      PassRefPtr<AnimatableValue> end,
      CSSPropertyID id) {
    return adoptRef(new LegacyStyleInterpolation(
        InterpolableAnimatableValue::create(std::move(start)),
        InterpolableAnimatableValue::create(std::move(end)), id));
  }

  // 1) convert m_cachedValue into an X
  // 2) shove X into StyleResolverState
  // X can be:
  // (1) a CSSValue (and applied via StyleBuilder::applyProperty)
  // (2) an AnimatableValue (and applied via
  //     AnimatedStyleBuilder::applyProperty)
  // (3) a custom value that is inserted directly into the StyleResolverState.
  void apply(StyleResolverState& state) const {
    AnimatedStyleBuilder::applyProperty(id(), state, currentValue().get());
  }

  bool isLegacyStyleInterpolation() const final { return true; }

  PassRefPtr<AnimatableValue> currentValue() const {
    return toInterpolableAnimatableValue(m_cachedValue.get())->value();
  }

  CSSPropertyID id() const { return m_property.cssProperty(); }

  const PropertyHandle& getProperty() const final { return m_property; }

  void interpolate(int iteration, double fraction) final;

 protected:
  LegacyStyleInterpolation(std::unique_ptr<InterpolableValue> start,
                           std::unique_ptr<InterpolableValue> end,
                           CSSPropertyID);

 private:
  const std::unique_ptr<InterpolableValue> m_start;
  const std::unique_ptr<InterpolableValue> m_end;
  PropertyHandle m_property;

  mutable double m_cachedFraction;
  mutable int m_cachedIteration;
  mutable std::unique_ptr<InterpolableValue> m_cachedValue;

  InterpolableValue* getCachedValueForTesting() const {
    return m_cachedValue.get();
  }

  friend class AnimationInterpolableValueTest;
  friend class AnimationInterpolationEffectTest;
};

DEFINE_TYPE_CASTS(LegacyStyleInterpolation,
                  Interpolation,
                  value,
                  value->isLegacyStyleInterpolation(),
                  value.isLegacyStyleInterpolation());

}  // namespace blink

#endif
