// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSNumberInterpolationType.h"

#include <memory>
#include "core/animation/NumberPropertyFunctions.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

class InheritedNumberChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<InheritedNumberChecker> Create(CSSPropertyID property,
                                                        double number) {
    return WTF::WrapUnique(new InheritedNumberChecker(property, number));
  }

 private:
  InheritedNumberChecker(CSSPropertyID property, double number)
      : property_(property), number_(number) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    double parent_number;
    if (!NumberPropertyFunctions::GetNumber(property_, *state.ParentStyle(),
                                            parent_number))
      return false;
    return parent_number == number_;
  }

  const CSSPropertyID property_;
  const double number_;
};

const CSSValue* CSSNumberInterpolationType::CreateCSSValue(
    const InterpolableValue& value,
    const NonInterpolableValue*,
    const StyleResolverState&) const {
  double number = ToInterpolableNumber(value).Value();
  return CSSPrimitiveValue::Create(round_to_integer_ ? round(number) : number,
                                   CSSPrimitiveValue::UnitType::kNumber);
}

InterpolationValue CSSNumberInterpolationType::CreateNumberValue(
    double number) const {
  return InterpolationValue(InterpolableNumber::Create(number));
}

InterpolationValue CSSNumberInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return CreateNumberValue(0);
}

InterpolationValue CSSNumberInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  double initial_number;
  if (!NumberPropertyFunctions::GetInitialNumber(CssProperty(), initial_number))
    return nullptr;
  return CreateNumberValue(initial_number);
}

InterpolationValue CSSNumberInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;
  double inherited_number;
  if (!NumberPropertyFunctions::GetNumber(CssProperty(), *state.ParentStyle(),
                                          inherited_number))
    return nullptr;
  conversion_checkers.push_back(
      InheritedNumberChecker::Create(CssProperty(), inherited_number));
  return CreateNumberValue(inherited_number);
}

InterpolationValue CSSNumberInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.IsPrimitiveValue() || !ToCSSPrimitiveValue(value).IsNumber())
    return nullptr;
  return CreateNumberValue(ToCSSPrimitiveValue(value).GetDoubleValue());
}

InterpolationValue
CSSNumberInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  double underlying_number;
  if (!NumberPropertyFunctions::GetNumber(CssProperty(), style,
                                          underlying_number))
    return nullptr;
  return CreateNumberValue(underlying_number);
}

void CSSNumberInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  double clamped_number = NumberPropertyFunctions::ClampNumber(
      CssProperty(), ToInterpolableNumber(interpolable_value).Value());
  if (!NumberPropertyFunctions::SetNumber(CssProperty(), *state.Style(),
                                          clamped_number))
    StyleBuilder::ApplyProperty(
        CssProperty(), state,
        *CSSPrimitiveValue::Create(clamped_number,
                                   CSSPrimitiveValue::UnitType::kNumber));
}

}  // namespace blink
