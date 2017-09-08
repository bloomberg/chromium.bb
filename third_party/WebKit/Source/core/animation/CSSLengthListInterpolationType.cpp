// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSLengthListInterpolationType.h"

#include <memory>
#include "core/animation/LengthInterpolationFunctions.h"
#include "core/animation/LengthListPropertyFunctions.h"
#include "core/animation/ListInterpolationFunctions.h"
#include "core/animation/UnderlyingLengthChecker.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/ComputedStyle.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

CSSLengthListInterpolationType::CSSLengthListInterpolationType(
    PropertyHandle property)
    : CSSInterpolationType(property),
      value_range_(LengthListPropertyFunctions::GetValueRange(CssProperty())) {}

InterpolationValue CSSLengthListInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  size_t underlying_length =
      UnderlyingLengthChecker::GetUnderlyingLength(underlying);
  conversion_checkers.push_back(
      UnderlyingLengthChecker::Create(underlying_length));

  if (underlying_length == 0)
    return nullptr;

  return ListInterpolationFunctions::CreateList(underlying_length, [](size_t) {
    return InterpolationValue(
        LengthInterpolationFunctions::CreateNeutralInterpolableValue());
  });
}

static InterpolationValue MaybeConvertLengthList(
    const Vector<Length>& length_list,
    float zoom) {
  if (length_list.IsEmpty())
    return nullptr;

  return ListInterpolationFunctions::CreateList(
      length_list.size(), [&length_list, zoom](size_t index) {
        return LengthInterpolationFunctions::MaybeConvertLength(
            length_list[index], zoom);
      });
}

InterpolationValue CSSLengthListInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  Vector<Length> initial_length_list;
  if (!LengthListPropertyFunctions::GetInitialLengthList(CssProperty(),
                                                         initial_length_list))
    return nullptr;
  return MaybeConvertLengthList(initial_length_list, 1);
}

class InheritedLengthListChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  ~InheritedLengthListChecker() final {}

  static std::unique_ptr<InheritedLengthListChecker> Create(
      CSSPropertyID property,
      const Vector<Length>& inherited_length_list) {
    return WTF::WrapUnique(
        new InheritedLengthListChecker(property, inherited_length_list));
  }

 private:
  InheritedLengthListChecker(CSSPropertyID property,
                             const Vector<Length>& inherited_length_list)
      : property_(property), inherited_length_list_(inherited_length_list) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    Vector<Length> inherited_length_list;
    LengthListPropertyFunctions::GetLengthList(property_, *state.ParentStyle(),
                                               inherited_length_list);
    return inherited_length_list_ == inherited_length_list;
  }

  CSSPropertyID property_;
  Vector<Length> inherited_length_list_;
};

InterpolationValue CSSLengthListInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  Vector<Length> inherited_length_list;
  bool success = LengthListPropertyFunctions::GetLengthList(
      CssProperty(), *state.ParentStyle(), inherited_length_list);
  conversion_checkers.push_back(
      InheritedLengthListChecker::Create(CssProperty(), inherited_length_list));
  if (!success)
    return nullptr;
  return MaybeConvertLengthList(inherited_length_list,
                                state.ParentStyle()->EffectiveZoom());
}

InterpolationValue CSSLengthListInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.IsBaseValueList())
    return nullptr;

  const CSSValueList& list = ToCSSValueList(value);
  return ListInterpolationFunctions::CreateList(
      list.length(), [&list](size_t index) {
        return LengthInterpolationFunctions::MaybeConvertCSSValue(
            list.Item(index));
      });
}

PairwiseInterpolationValue CSSLengthListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return ListInterpolationFunctions::MaybeMergeSingles(
      std::move(start), std::move(end),
      ListInterpolationFunctions::LengthMatchingStrategy::kLowestCommonMultiple,
      LengthInterpolationFunctions::MergeSingles);
}

InterpolationValue
CSSLengthListInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  Vector<Length> underlying_length_list;
  if (!LengthListPropertyFunctions::GetLengthList(CssProperty(), style,
                                                  underlying_length_list))
    return nullptr;
  return MaybeConvertLengthList(underlying_length_list, style.EffectiveZoom());
}

void CSSLengthListInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  ListInterpolationFunctions::Composite(
      underlying_value_owner, underlying_fraction, *this, value,
      ListInterpolationFunctions::LengthMatchingStrategy::kLowestCommonMultiple,
      LengthInterpolationFunctions::NonInterpolableValuesAreCompatible,
      LengthInterpolationFunctions::Composite);
}

void CSSLengthListInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const InterpolableList& interpolable_list =
      ToInterpolableList(interpolable_value);
  const size_t length = interpolable_list.length();
  DCHECK_GT(length, 0U);
  const NonInterpolableList& non_interpolable_list =
      ToNonInterpolableList(*non_interpolable_value);
  DCHECK_EQ(non_interpolable_list.length(), length);
  Vector<Length> result(length);
  for (size_t i = 0; i < length; i++) {
    result[i] = LengthInterpolationFunctions::CreateLength(
        *interpolable_list.Get(i), non_interpolable_list.Get(i),
        state.CssToLengthConversionData(), value_range_);
  }
  LengthListPropertyFunctions::SetLengthList(CssProperty(), *state.Style(),
                                             std::move(result));
}

}  // namespace blink
