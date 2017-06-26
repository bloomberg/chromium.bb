// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSLengthInterpolationType.h"

#include <memory>
#include "core/animation/LengthInterpolationFunctions.h"
#include "core/animation/LengthPropertyFunctions.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "platform/LengthFunctions.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

CSSLengthInterpolationType::CSSLengthInterpolationType(
    PropertyHandle property,
    const PropertyRegistration* registration)
    : CSSInterpolationType(property, registration),
      value_range_(LengthPropertyFunctions::GetValueRange(CssProperty())) {}

float CSSLengthInterpolationType::EffectiveZoom(
    const ComputedStyle& style) const {
  return LengthPropertyFunctions::IsZoomedLength(CssProperty())
             ? style.EffectiveZoom()
             : 1;
}

class InheritedLengthChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<InheritedLengthChecker> Create(CSSPropertyID property,
                                                        const Length& length) {
    return WTF::WrapUnique(new InheritedLengthChecker(property, length));
  }

 private:
  InheritedLengthChecker(CSSPropertyID property, const Length& length)
      : property_(property), length_(length) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    Length parent_length;
    if (!LengthPropertyFunctions::GetLength(property_, *state.ParentStyle(),
                                            parent_length))
      return false;
    return parent_length == length_;
  }

  const CSSPropertyID property_;
  const Length length_;
};

InterpolationValue CSSLengthInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(
      LengthInterpolationFunctions::CreateNeutralInterpolableValue());
}

InterpolationValue CSSLengthInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  Length initial_length;
  if (!LengthPropertyFunctions::GetInitialLength(CssProperty(), initial_length))
    return nullptr;
  return LengthInterpolationFunctions::MaybeConvertLength(initial_length, 1);
}

InterpolationValue CSSLengthInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;
  Length inherited_length;
  if (!LengthPropertyFunctions::GetLength(CssProperty(), *state.ParentStyle(),
                                          inherited_length))
    return nullptr;
  conversion_checkers.push_back(
      InheritedLengthChecker::Create(CssProperty(), inherited_length));
  return LengthInterpolationFunctions::MaybeConvertLength(
      inherited_length, EffectiveZoom(*state.ParentStyle()));
}

InterpolationValue CSSLengthInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers& conversion_checkers) const {
  if (value.IsIdentifierValue()) {
    CSSValueID value_id = ToCSSIdentifierValue(value).GetValueID();
    double pixels;
    if (!LengthPropertyFunctions::GetPixelsForKeyword(CssProperty(), value_id,
                                                      pixels))
      return nullptr;
    return InterpolationValue(
        LengthInterpolationFunctions::CreateInterpolablePixels(pixels));
  }

  return LengthInterpolationFunctions::MaybeConvertCSSValue(value);
}

PairwiseInterpolationValue CSSLengthInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return LengthInterpolationFunctions::MergeSingles(std::move(start),
                                                    std::move(end));
}

InterpolationValue
CSSLengthInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  Length underlying_length;
  if (!LengthPropertyFunctions::GetLength(CssProperty(), style,
                                          underlying_length))
    return nullptr;
  return LengthInterpolationFunctions::MaybeConvertLength(underlying_length,
                                                          EffectiveZoom(style));
}

const CSSValue* CSSLengthInterpolationType::CreateCSSValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    const StyleResolverState&) const {
  return LengthInterpolationFunctions::CreateCSSValue(
      interpolable_value, non_interpolable_value, value_range_);
}

void CSSLengthInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  InterpolationValue& underlying = underlying_value_owner.MutableValue();
  LengthInterpolationFunctions::Composite(
      underlying.interpolable_value, underlying.non_interpolable_value,
      underlying_fraction, *value.interpolable_value,
      value.non_interpolable_value.Get());
}

void CSSLengthInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  ComputedStyle& style = *state.Style();
  float zoom = EffectiveZoom(style);
  Length length = LengthInterpolationFunctions::CreateLength(
      interpolable_value, non_interpolable_value,
      state.CssToLengthConversionData(), value_range_);
  if (LengthPropertyFunctions::SetLength(CssProperty(), style, length)) {
#if DCHECK_IS_ON()
    // Assert that setting the length on ComputedStyle directly is identical to
    // the StyleBuilder code path. This check is useful for catching differences
    // in clamping behaviour.
    Length before;
    Length after;
    DCHECK(LengthPropertyFunctions::GetLength(CssProperty(), style, before));
    StyleBuilder::ApplyProperty(CssProperty(), state,
                                *CSSValue::Create(length, zoom));
    DCHECK(LengthPropertyFunctions::GetLength(CssProperty(), style, after));
    DCHECK(before.IsSpecified());
    DCHECK(after.IsSpecified());
    const float kSlack = 0.1;
    float delta =
        FloatValueForLength(after, 100) - FloatValueForLength(before, 100);
    DCHECK_LT(std::abs(delta), kSlack);
#endif
    return;
  }
  StyleBuilder::ApplyProperty(CssProperty(), state,
                              *CSSValue::Create(length, zoom));
}

}  // namespace blink
