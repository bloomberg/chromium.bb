// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSOffsetRotateInterpolationType.h"

#include <memory>
#include "core/css/resolver/StyleBuilderConverter.h"
#include "core/style/ComputedStyle.h"
#include "core/style/StyleOffsetRotation.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

class CSSOffsetRotationNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSOffsetRotationNonInterpolableValue() {}

  static RefPtr<CSSOffsetRotationNonInterpolableValue> Create(
      OffsetRotationType rotation_type) {
    return WTF::AdoptRef(
        new CSSOffsetRotationNonInterpolableValue(rotation_type));
  }

  OffsetRotationType RotationType() const { return rotation_type_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSOffsetRotationNonInterpolableValue(OffsetRotationType rotation_type)
      : rotation_type_(rotation_type) {}

  OffsetRotationType rotation_type_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSOffsetRotationNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSOffsetRotationNonInterpolableValue);

namespace {

class UnderlyingRotationTypeChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<UnderlyingRotationTypeChecker> Create(
      OffsetRotationType underlying_rotation_type) {
    return WTF::WrapUnique(
        new UnderlyingRotationTypeChecker(underlying_rotation_type));
  }

  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return underlying_rotation_type_ == ToCSSOffsetRotationNonInterpolableValue(
                                            *underlying.non_interpolable_value)
                                            .RotationType();
  }

 private:
  UnderlyingRotationTypeChecker(OffsetRotationType underlying_rotation_type)
      : underlying_rotation_type_(underlying_rotation_type) {}

  OffsetRotationType underlying_rotation_type_;
};

class InheritedRotationTypeChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<InheritedRotationTypeChecker> Create(
      OffsetRotationType inherited_rotation_type) {
    return WTF::WrapUnique(
        new InheritedRotationTypeChecker(inherited_rotation_type));
  }

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return inherited_rotation_type_ == state.ParentStyle()->OffsetRotate().type;
  }

 private:
  InheritedRotationTypeChecker(OffsetRotationType inherited_rotation_type)
      : inherited_rotation_type_(inherited_rotation_type) {}

  OffsetRotationType inherited_rotation_type_;
};

InterpolationValue ConvertOffsetRotate(const StyleOffsetRotation& rotation) {
  return InterpolationValue(
      InterpolableNumber::Create(rotation.angle),
      CSSOffsetRotationNonInterpolableValue::Create(rotation.type));
}

}  // namespace

InterpolationValue CSSOffsetRotateInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  OffsetRotationType underlying_rotation_type =
      ToCSSOffsetRotationNonInterpolableValue(
          *underlying.non_interpolable_value)
          .RotationType();
  conversion_checkers.push_back(
      UnderlyingRotationTypeChecker::Create(underlying_rotation_type));
  return ConvertOffsetRotate(StyleOffsetRotation(0, underlying_rotation_type));
}

InterpolationValue CSSOffsetRotateInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  return ConvertOffsetRotate(StyleOffsetRotation(0, kOffsetRotationAuto));
}

InterpolationValue CSSOffsetRotateInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  OffsetRotationType inherited_rotation_type =
      state.ParentStyle()->OffsetRotate().type;
  conversion_checkers.push_back(
      InheritedRotationTypeChecker::Create(inherited_rotation_type));
  return ConvertOffsetRotate(state.ParentStyle()->OffsetRotate());
}

InterpolationValue CSSOffsetRotateInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  return ConvertOffsetRotate(StyleBuilderConverter::ConvertOffsetRotate(value));
}

PairwiseInterpolationValue CSSOffsetRotateInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const OffsetRotationType& start_type =
      ToCSSOffsetRotationNonInterpolableValue(*start.non_interpolable_value)
          .RotationType();
  const OffsetRotationType& end_type =
      ToCSSOffsetRotationNonInterpolableValue(*end.non_interpolable_value)
          .RotationType();
  if (start_type != end_type)
    return nullptr;
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

InterpolationValue
CSSOffsetRotateInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return ConvertOffsetRotate(style.OffsetRotate());
}

void CSSOffsetRotateInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  const OffsetRotationType& underlying_type =
      ToCSSOffsetRotationNonInterpolableValue(
          *underlying_value_owner.Value().non_interpolable_value)
          .RotationType();
  const OffsetRotationType& rotation_type =
      ToCSSOffsetRotationNonInterpolableValue(*value.non_interpolable_value)
          .RotationType();
  if (underlying_type == rotation_type) {
    underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
        underlying_fraction, *value.interpolable_value);
  } else {
    underlying_value_owner.Set(*this, value);
  }
}

void CSSOffsetRotateInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  state.Style()->SetOffsetRotate(StyleOffsetRotation(
      ToInterpolableNumber(interpolable_value).Value(),
      ToCSSOffsetRotationNonInterpolableValue(*non_interpolable_value)
          .RotationType()));
}

}  // namespace blink
