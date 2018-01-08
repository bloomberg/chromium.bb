// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSImageSliceInterpolationType.h"

#include <memory>
#include "core/animation/CSSLengthInterpolationType.h"
#include "core/animation/ImageSlicePropertyFunctions.h"
#include "core/animation/SideIndex.h"
#include "core/css/CSSBorderImageSliceValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

namespace {

struct SliceTypes {
  explicit SliceTypes(const ImageSlice& slice) {
    is_number[kSideTop] = slice.slices.Top().IsFixed();
    is_number[kSideRight] = slice.slices.Right().IsFixed();
    is_number[kSideBottom] = slice.slices.Bottom().IsFixed();
    is_number[kSideLeft] = slice.slices.Left().IsFixed();
    fill = slice.fill;
  }
  explicit SliceTypes(const cssvalue::CSSBorderImageSliceValue& slice) {
    is_number[kSideTop] = slice.Slices().Top()->IsPrimitiveValue() &&
                          ToCSSPrimitiveValue(slice.Slices().Top())->IsNumber();
    is_number[kSideRight] =
        slice.Slices().Right()->IsPrimitiveValue() &&
        ToCSSPrimitiveValue(slice.Slices().Right())->IsNumber();
    is_number[kSideBottom] =
        slice.Slices().Bottom()->IsPrimitiveValue() &&
        ToCSSPrimitiveValue(slice.Slices().Bottom())->IsNumber();
    is_number[kSideLeft] =
        slice.Slices().Left()->IsPrimitiveValue() &&
        ToCSSPrimitiveValue(slice.Slices().Left())->IsNumber();
    fill = slice.Fill();
  }

  bool operator==(const SliceTypes& other) const {
    for (size_t i = 0; i < kSideIndexCount; i++) {
      if (is_number[i] != other.is_number[i])
        return false;
    }
    return fill == other.fill;
  }
  bool operator!=(const SliceTypes& other) const { return !(*this == other); }

  // If a side is not a number then it is a percentage.
  bool is_number[kSideIndexCount];
  bool fill;
};

}  // namespace

class CSSImageSliceNonInterpolableValue : public NonInterpolableValue {
 public:
  static scoped_refptr<CSSImageSliceNonInterpolableValue> Create(
      const SliceTypes& types) {
    return base::AdoptRef(new CSSImageSliceNonInterpolableValue(types));
  }

  const SliceTypes& Types() const { return types_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSImageSliceNonInterpolableValue(const SliceTypes& types) : types_(types) {}

  const SliceTypes types_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSImageSliceNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSImageSliceNonInterpolableValue);

namespace {

class UnderlyingSliceTypesChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<UnderlyingSliceTypesChecker> Create(
      const SliceTypes& underlying_types) {
    return WTF::WrapUnique(new UnderlyingSliceTypesChecker(underlying_types));
  }

  static SliceTypes GetUnderlyingSliceTypes(
      const InterpolationValue& underlying) {
    return ToCSSImageSliceNonInterpolableValue(
               *underlying.non_interpolable_value)
        .Types();
  }

 private:
  UnderlyingSliceTypesChecker(const SliceTypes& underlying_types)
      : underlying_types_(underlying_types) {}

  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    return underlying_types_ == GetUnderlyingSliceTypes(underlying);
  }

  const SliceTypes underlying_types_;
};

class InheritedSliceTypesChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<InheritedSliceTypesChecker> Create(
      const CSSProperty& property,
      const SliceTypes& inherited_types) {
    return WTF::WrapUnique(
        new InheritedSliceTypesChecker(property, inherited_types));
  }

 private:
  InheritedSliceTypesChecker(const CSSProperty& property,
                             const SliceTypes& inherited_types)
      : property_(property), inherited_types_(inherited_types) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return inherited_types_ ==
           SliceTypes(ImageSlicePropertyFunctions::GetImageSlice(
               property_, *state.ParentStyle()));
  }

  const CSSProperty& property_;
  const SliceTypes inherited_types_;
};

InterpolationValue ConvertImageSlice(const ImageSlice& slice, double zoom) {
  std::unique_ptr<InterpolableList> list =
      InterpolableList::Create(kSideIndexCount);
  const Length* sides[kSideIndexCount] = {};
  sides[kSideTop] = &slice.slices.Top();
  sides[kSideRight] = &slice.slices.Right();
  sides[kSideBottom] = &slice.slices.Bottom();
  sides[kSideLeft] = &slice.slices.Left();

  for (size_t i = 0; i < kSideIndexCount; i++) {
    const Length& side = *sides[i];
    list->Set(i, InterpolableNumber::Create(
                     side.IsFixed() ? side.Pixels() / zoom : side.Percent()));
  }

  return InterpolationValue(
      std::move(list),
      CSSImageSliceNonInterpolableValue::Create(SliceTypes(slice)));
}

}  // namespace

InterpolationValue CSSImageSliceInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  SliceTypes underlying_types =
      UnderlyingSliceTypesChecker::GetUnderlyingSliceTypes(underlying);
  conversion_checkers.push_back(
      UnderlyingSliceTypesChecker::Create(underlying_types));
  LengthBox zero_box(
      Length(0, underlying_types.is_number[kSideTop] ? kFixed : kPercent),
      Length(0, underlying_types.is_number[kSideRight] ? kFixed : kPercent),
      Length(0, underlying_types.is_number[kSideBottom] ? kFixed : kPercent),
      Length(0, underlying_types.is_number[kSideLeft] ? kFixed : kPercent));
  return ConvertImageSlice(ImageSlice(zero_box, underlying_types.fill), 1);
}

InterpolationValue CSSImageSliceInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  return ConvertImageSlice(
      ImageSlicePropertyFunctions::GetInitialImageSlice(CssProperty()), 1);
}

InterpolationValue CSSImageSliceInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const ImageSlice& inherited_image_slice =
      ImageSlicePropertyFunctions::GetImageSlice(CssProperty(),
                                                 *state.ParentStyle());
  conversion_checkers.push_back(InheritedSliceTypesChecker::Create(
      CssProperty(), SliceTypes(inherited_image_slice)));
  return ConvertImageSlice(inherited_image_slice,
                           state.ParentStyle()->EffectiveZoom());
}

InterpolationValue CSSImageSliceInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.IsBorderImageSliceValue())
    return nullptr;

  const cssvalue::CSSBorderImageSliceValue& slice =
      cssvalue::ToCSSBorderImageSliceValue(value);
  std::unique_ptr<InterpolableList> list =
      InterpolableList::Create(kSideIndexCount);
  const CSSValue* sides[kSideIndexCount];
  sides[kSideTop] = slice.Slices().Top();
  sides[kSideRight] = slice.Slices().Right();
  sides[kSideBottom] = slice.Slices().Bottom();
  sides[kSideLeft] = slice.Slices().Left();

  for (size_t i = 0; i < kSideIndexCount; i++) {
    const CSSPrimitiveValue& side = *ToCSSPrimitiveValue(sides[i]);
    DCHECK(side.IsNumber() || side.IsPercentage());
    list->Set(i, InterpolableNumber::Create(side.GetDoubleValue()));
  }

  return InterpolationValue(
      std::move(list),
      CSSImageSliceNonInterpolableValue::Create(SliceTypes(slice)));
}

InterpolationValue
CSSImageSliceInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return ConvertImageSlice(
      ImageSlicePropertyFunctions::GetImageSlice(CssProperty(), style),
      style.EffectiveZoom());
}

PairwiseInterpolationValue CSSImageSliceInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const SliceTypes& start_slice_types =
      ToCSSImageSliceNonInterpolableValue(*start.non_interpolable_value)
          .Types();
  const SliceTypes& end_slice_types =
      ToCSSImageSliceNonInterpolableValue(*end.non_interpolable_value).Types();

  if (start_slice_types != end_slice_types)
    return nullptr;

  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

void CSSImageSliceInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  const SliceTypes& underlying_types =
      ToCSSImageSliceNonInterpolableValue(
          *underlying_value_owner.Value().non_interpolable_value)
          .Types();
  const SliceTypes& types =
      ToCSSImageSliceNonInterpolableValue(*value.non_interpolable_value)
          .Types();

  if (underlying_types == types)
    underlying_value_owner.MutableValue().interpolable_value->ScaleAndAdd(
        underlying_fraction, *value.interpolable_value);
  else
    underlying_value_owner.Set(*this, value);
}

void CSSImageSliceInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  ComputedStyle& style = *state.Style();
  const InterpolableList& list = ToInterpolableList(interpolable_value);
  const SliceTypes& types =
      ToCSSImageSliceNonInterpolableValue(non_interpolable_value)->Types();
  const auto& convert_side = [&types, &list, &style](size_t index) {
    float value =
        clampTo<float>(ToInterpolableNumber(list.Get(index))->Value(), 0);
    return types.is_number[index]
               ? Length(value * style.EffectiveZoom(), kFixed)
               : Length(value, kPercent);
  };
  LengthBox box(convert_side(kSideTop), convert_side(kSideRight),
                convert_side(kSideBottom), convert_side(kSideLeft));
  ImageSlicePropertyFunctions::SetImageSlice(CssProperty(), style,
                                             ImageSlice(box, types.fill));
}

}  // namespace blink
