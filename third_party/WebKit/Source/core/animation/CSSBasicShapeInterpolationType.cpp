// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSBasicShapeInterpolationType.h"

#include <memory>
#include "core/animation/BasicShapeInterpolationFunctions.h"
#include "core/animation/BasicShapePropertyFunctions.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/BasicShapes.h"
#include "core/style/DataEquivalency.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

namespace {

class UnderlyingCompatibilityChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<UnderlyingCompatibilityChecker> Create(
      PassRefPtr<NonInterpolableValue> underlying_non_interpolable_value) {
    return WTF::WrapUnique(new UnderlyingCompatibilityChecker(
        std::move(underlying_non_interpolable_value)));
  }

 private:
  UnderlyingCompatibilityChecker(
      PassRefPtr<NonInterpolableValue> underlying_non_interpolable_value)
      : underlying_non_interpolable_value_(
            std::move(underlying_non_interpolable_value)) {}

  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return BasicShapeInterpolationFunctions::ShapesAreCompatible(
        *underlying_non_interpolable_value_,
        *underlying.non_interpolable_value);
  }

  RefPtr<NonInterpolableValue> underlying_non_interpolable_value_;
};

class InheritedShapeChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<InheritedShapeChecker> Create(
      CSSPropertyID property,
      PassRefPtr<BasicShape> inherited_shape) {
    return WTF::WrapUnique(
        new InheritedShapeChecker(property, std::move(inherited_shape)));
  }

 private:
  InheritedShapeChecker(CSSPropertyID property,
                        PassRefPtr<BasicShape> inherited_shape)
      : property_(property), inherited_shape_(std::move(inherited_shape)) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return DataEquivalent(inherited_shape_.Get(),
                          BasicShapePropertyFunctions::GetBasicShape(
                              property_, *state.ParentStyle()));
  }

  const CSSPropertyID property_;
  RefPtr<BasicShape> inherited_shape_;
};

}  // namespace

InterpolationValue CSSBasicShapeInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  // const_cast is for taking refs.
  NonInterpolableValue* non_interpolable_value =
      const_cast<NonInterpolableValue*>(
          underlying.non_interpolable_value.Get());
  conversion_checkers.push_back(
      UnderlyingCompatibilityChecker::Create(non_interpolable_value));
  return InterpolationValue(
      BasicShapeInterpolationFunctions::CreateNeutralValue(
          *underlying.non_interpolable_value),
      non_interpolable_value);
}

InterpolationValue CSSBasicShapeInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return BasicShapeInterpolationFunctions::MaybeConvertBasicShape(
      BasicShapePropertyFunctions::GetInitialBasicShape(CssProperty()), 1);
}

InterpolationValue CSSBasicShapeInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const BasicShape* shape = BasicShapePropertyFunctions::GetBasicShape(
      CssProperty(), *state.ParentStyle());
  // const_cast to take a ref.
  conversion_checkers.push_back(InheritedShapeChecker::Create(
      CssProperty(), const_cast<BasicShape*>(shape)));
  return BasicShapeInterpolationFunctions::MaybeConvertBasicShape(
      shape, state.ParentStyle()->EffectiveZoom());
}

InterpolationValue CSSBasicShapeInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.IsBaseValueList())
    return BasicShapeInterpolationFunctions::MaybeConvertCSSValue(value);

  const CSSValueList& list = ToCSSValueList(value);
  if (list.length() != 1)
    return nullptr;
  return BasicShapeInterpolationFunctions::MaybeConvertCSSValue(list.Item(0));
}

PairwiseInterpolationValue CSSBasicShapeInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  if (!BasicShapeInterpolationFunctions::ShapesAreCompatible(
          *start.non_interpolable_value, *end.non_interpolable_value))
    return nullptr;
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

InterpolationValue
CSSBasicShapeInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return BasicShapeInterpolationFunctions::MaybeConvertBasicShape(
      BasicShapePropertyFunctions::GetBasicShape(CssProperty(), style),
      style.EffectiveZoom());
}

void CSSBasicShapeInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  if (!BasicShapeInterpolationFunctions::ShapesAreCompatible(
          *underlying_value_owner.Value().non_interpolable_value,
          *value.non_interpolable_value)) {
    underlying_value_owner.Set(*this, value);
    return;
  }

  underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
      underlying_fraction, *value.interpolable_value);
}

void CSSBasicShapeInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  BasicShapePropertyFunctions::SetBasicShape(
      CssProperty(), *state.Style(),
      BasicShapeInterpolationFunctions::CreateBasicShape(
          interpolable_value, *non_interpolable_value,
          state.CssToLengthConversionData()));
}

}  // namespace blink
