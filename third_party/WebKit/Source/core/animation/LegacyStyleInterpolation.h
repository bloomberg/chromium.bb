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

class CORE_EXPORT StyleInterpolation : public Interpolation {
 public:
  // 1) convert m_cachedValue into an X
  // 2) shove X into StyleResolverState
  // X can be:
  // (1) a CSSValue (and applied via StyleBuilder::applyProperty)
  // (2) an AnimatableValue (and applied via
  //     AnimatedStyleBuilder::applyProperty)
  // (3) a custom value that is inserted directly into the StyleResolverState.
  virtual void apply(StyleResolverState&) const = 0;

  bool isStyleInterpolation() const final { return true; }

  CSSPropertyID id() const { return m_id; }

  PropertyHandle getProperty() const final { return PropertyHandle(id()); }

 protected:
  CSSPropertyID m_id;

  StyleInterpolation(std::unique_ptr<InterpolableValue> start,
                     std::unique_ptr<InterpolableValue> end,
                     CSSPropertyID id)
      : Interpolation(std::move(start), std::move(end)), m_id(id) {}
};

DEFINE_TYPE_CASTS(StyleInterpolation,
                  Interpolation,
                  value,
                  value->isStyleInterpolation(),
                  value.isStyleInterpolation());

class LegacyStyleInterpolation : public StyleInterpolation {
 public:
  static PassRefPtr<LegacyStyleInterpolation> create(
      PassRefPtr<AnimatableValue> start,
      PassRefPtr<AnimatableValue> end,
      CSSPropertyID id) {
    return adoptRef(new LegacyStyleInterpolation(
        InterpolableAnimatableValue::create(std::move(start)),
        InterpolableAnimatableValue::create(std::move(end)), id));
  }

  void apply(StyleResolverState& state) const override {
    AnimatedStyleBuilder::applyProperty(m_id, state, currentValue().get());
  }

  bool isLegacyStyleInterpolation() const final { return true; }
  PassRefPtr<AnimatableValue> currentValue() const {
    return toInterpolableAnimatableValue(m_cachedValue.get())->value();
  }

 private:
  LegacyStyleInterpolation(std::unique_ptr<InterpolableValue> start,
                           std::unique_ptr<InterpolableValue> end,
                           CSSPropertyID id)
      : StyleInterpolation(std::move(start), std::move(end), id) {}
};

DEFINE_TYPE_CASTS(LegacyStyleInterpolation,
                  Interpolation,
                  value,
                  value->isLegacyStyleInterpolation(),
                  value.isLegacyStyleInterpolation());

}  // namespace blink

#endif
