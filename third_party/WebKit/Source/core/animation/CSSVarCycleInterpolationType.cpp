// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSVarCycleInterpolationType.h"

#include "core/animation/CSSInterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/PropertyRegistration.h"
#include "core/css/resolver/CSSVariableResolver.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/style/ComputedStyle.h"

namespace blink {

class CycleChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<CycleChecker> Create(
      const CSSCustomPropertyDeclaration& declaration,
      bool cycle_detected) {
    return WTF::WrapUnique(new CycleChecker(declaration, cycle_detected));
  }

 private:
  CycleChecker(const CSSCustomPropertyDeclaration& declaration,
               bool cycle_detected)
      : declaration_(declaration), cycle_detected_(cycle_detected) {}

  bool IsValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    DCHECK(ToCSSInterpolationEnvironment(environment).HasVariableResolver());
    CSSVariableResolver& variable_resolver =
        ToCSSInterpolationEnvironment(environment).VariableResolver();
    bool cycle_detected = false;
    variable_resolver.ResolveCustomPropertyAnimationKeyframe(*declaration_,
                                                             cycle_detected);
    return cycle_detected == cycle_detected_;
  }

  Persistent<const CSSCustomPropertyDeclaration> declaration_;
  const bool cycle_detected_;
};

CSSVarCycleInterpolationType::CSSVarCycleInterpolationType(
    const PropertyHandle& property,
    const PropertyRegistration& registration)
    : InterpolationType(property), registration_(registration) {
  DCHECK(property.IsCSSCustomProperty());
}

static InterpolationValue CreateCycleDetectedValue() {
  return InterpolationValue(InterpolableList::Create(0));
}

InterpolationValue CSSVarCycleInterpolationType::MaybeConvertSingle(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  const CSSCustomPropertyDeclaration& declaration =
      *ToCSSCustomPropertyDeclaration(
          ToCSSPropertySpecificKeyframe(keyframe).Value());
  DCHECK_EQ(GetProperty().CustomPropertyName(), declaration.GetName());
  if (!declaration.Value() || !declaration.Value()->NeedsVariableResolution()) {
    return nullptr;
  }

  DCHECK(ToCSSInterpolationEnvironment(environment).HasVariableResolver());
  CSSVariableResolver& variable_resolver =
      ToCSSInterpolationEnvironment(environment).VariableResolver();
  bool cycle_detected = false;
  variable_resolver.ResolveCustomPropertyAnimationKeyframe(declaration,
                                                           cycle_detected);
  conversion_checkers.push_back(
      CycleChecker::Create(declaration, cycle_detected));
  return cycle_detected ? CreateCycleDetectedValue() : nullptr;
}

static bool IsCycleDetected(const InterpolationValue& value) {
  return static_cast<bool>(value);
}

PairwiseInterpolationValue CSSVarCycleInterpolationType::MaybeConvertPairwise(
    const PropertySpecificKeyframe& start_keyframe,
    const PropertySpecificKeyframe& end_keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversionCheckers) const {
  InterpolationValue start = MaybeConvertSingle(start_keyframe, environment,
                                                underlying, conversionCheckers);
  InterpolationValue end = MaybeConvertSingle(end_keyframe, environment,
                                              underlying, conversionCheckers);
  if (!IsCycleDetected(start) && !IsCycleDetected(end)) {
    return nullptr;
  }
  // If either keyframe has a cyclic dependency then the entire interpolation
  // unsets the custom property.
  if (!start) {
    start = CreateCycleDetectedValue();
  }
  if (!end) {
    end = CreateCycleDetectedValue();
  }
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value));
}

InterpolationValue CSSVarCycleInterpolationType::MaybeConvertUnderlyingValue(
    const InterpolationEnvironment& environment) const {
  const ComputedStyle& style =
      ToCSSInterpolationEnvironment(environment).Style();
  DCHECK(!style.GetVariable(GetProperty().CustomPropertyName()) ||
         !style.GetVariable(GetProperty().CustomPropertyName())
              ->NeedsVariableResolution());
  return nullptr;
}

void CSSVarCycleInterpolationType::Apply(
    const InterpolableValue&,
    const NonInterpolableValue*,
    InterpolationEnvironment& environment) const {
  StyleBuilder::ApplyProperty(
      GetProperty().CssProperty(),
      ToCSSInterpolationEnvironment(environment).GetState(),
      *CSSCustomPropertyDeclaration::Create(GetProperty().CustomPropertyName(),
                                            CSSValueUnset));
}

}  // namespace blink
