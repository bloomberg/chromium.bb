// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SizeInterpolationFunctions.h"

#include "core/animation/LengthInterpolationFunctions.h"
#include "core/animation/UnderlyingValueOwner.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/css/CSSValuePair.h"

namespace blink {

class CSSSizeNonInterpolableValue : public NonInterpolableValue {
 public:
  static RefPtr<CSSSizeNonInterpolableValue> Create(CSSValueID keyword) {
    return AdoptRef(new CSSSizeNonInterpolableValue(keyword));
  }

  static RefPtr<CSSSizeNonInterpolableValue> Create(
      RefPtr<NonInterpolableValue> length_non_interpolable_value) {
    return AdoptRef(new CSSSizeNonInterpolableValue(
        std::move(length_non_interpolable_value)));
  }

  bool IsKeyword() const { return keyword_ != CSSValueInvalid; }
  CSSValueID Keyword() const {
    DCHECK(IsKeyword());
    return keyword_;
  }

  const NonInterpolableValue* LengthNonInterpolableValue() const {
    DCHECK(!IsKeyword());
    return length_non_interpolable_value_.Get();
  }
  RefPtr<NonInterpolableValue>& LengthNonInterpolableValue() {
    DCHECK(!IsKeyword());
    return length_non_interpolable_value_;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSSizeNonInterpolableValue(CSSValueID keyword)
      : keyword_(keyword), length_non_interpolable_value_(nullptr) {
    DCHECK_NE(keyword, CSSValueInvalid);
  }

  CSSSizeNonInterpolableValue(
      RefPtr<NonInterpolableValue> length_non_interpolable_value)
      : keyword_(CSSValueInvalid),
        length_non_interpolable_value_(
            std::move(length_non_interpolable_value)) {}

  CSSValueID keyword_;
  RefPtr<NonInterpolableValue> length_non_interpolable_value_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSSizeNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSSizeNonInterpolableValue);

static InterpolationValue ConvertKeyword(CSSValueID keyword) {
  return InterpolationValue(InterpolableList::Create(0),
                            CSSSizeNonInterpolableValue::Create(keyword));
}

static InterpolationValue WrapConvertedLength(
    InterpolationValue&& converted_length) {
  if (!converted_length)
    return nullptr;
  return InterpolationValue(std::move(converted_length.interpolable_value),
                            CSSSizeNonInterpolableValue::Create(std::move(
                                converted_length.non_interpolable_value)));
}

InterpolationValue SizeInterpolationFunctions::ConvertFillSizeSide(
    const FillSize& fill_size,
    float zoom,
    bool convert_width) {
  switch (fill_size.type) {
    case kSizeLength: {
      const Length& side =
          convert_width ? fill_size.size.Width() : fill_size.size.Height();
      if (side.IsAuto())
        return ConvertKeyword(CSSValueAuto);
      return WrapConvertedLength(
          LengthInterpolationFunctions::MaybeConvertLength(side, zoom));
    }
    case kContain:
      return ConvertKeyword(CSSValueContain);
    case kCover:
      return ConvertKeyword(CSSValueCover);
    case kSizeNone:
    default:
      NOTREACHED();
      return nullptr;
  }
}

InterpolationValue SizeInterpolationFunctions::MaybeConvertCSSSizeSide(
    const CSSValue& value,
    bool convert_width) {
  if (value.IsValuePair()) {
    const CSSValuePair& pair = ToCSSValuePair(value);
    const CSSValue& side = convert_width ? pair.First() : pair.Second();
    if (side.IsIdentifierValue() &&
        ToCSSIdentifierValue(side).GetValueID() == CSSValueAuto)
      return ConvertKeyword(CSSValueAuto);
    return WrapConvertedLength(
        LengthInterpolationFunctions::MaybeConvertCSSValue(side));
  }

  if (!value.IsIdentifierValue() && !value.IsPrimitiveValue())
    return nullptr;
  if (value.IsIdentifierValue())
    return ConvertKeyword(ToCSSIdentifierValue(value).GetValueID());

  // A single length is equivalent to "<length> auto".
  if (convert_width)
    return WrapConvertedLength(
        LengthInterpolationFunctions::MaybeConvertCSSValue(value));
  return ConvertKeyword(CSSValueAuto);
}

PairwiseInterpolationValue SizeInterpolationFunctions::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) {
  if (!NonInterpolableValuesAreCompatible(start.non_interpolable_value.Get(),
                                          end.non_interpolable_value.Get()))
    return nullptr;
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

InterpolationValue SizeInterpolationFunctions::CreateNeutralValue(
    const NonInterpolableValue* non_interpolable_value) {
  auto& size = ToCSSSizeNonInterpolableValue(*non_interpolable_value);
  if (size.IsKeyword())
    return ConvertKeyword(size.Keyword());
  return WrapConvertedLength(InterpolationValue(
      LengthInterpolationFunctions::CreateNeutralInterpolableValue()));
}

bool SizeInterpolationFunctions::NonInterpolableValuesAreCompatible(
    const NonInterpolableValue* a,
    const NonInterpolableValue* b) {
  const auto& size_a = ToCSSSizeNonInterpolableValue(*a);
  const auto& size_b = ToCSSSizeNonInterpolableValue(*b);
  if (size_a.IsKeyword() != size_b.IsKeyword())
    return false;
  if (size_a.IsKeyword())
    return size_a.Keyword() == size_b.Keyword();
  return true;
}

void SizeInterpolationFunctions::Composite(
    std::unique_ptr<InterpolableValue>& underlying_interpolable_value,
    RefPtr<NonInterpolableValue>& underlying_non_interpolable_value,
    double underlying_fraction,
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value) {
  const auto& size_non_interpolable_value =
      ToCSSSizeNonInterpolableValue(*non_interpolable_value);
  if (size_non_interpolable_value.IsKeyword())
    return;
  auto& underlying_size_non_interpolable_value =
      ToCSSSizeNonInterpolableValue(*underlying_non_interpolable_value);
  LengthInterpolationFunctions::Composite(
      underlying_interpolable_value,
      underlying_size_non_interpolable_value.LengthNonInterpolableValue(),
      underlying_fraction, interpolable_value,
      size_non_interpolable_value.LengthNonInterpolableValue());
}

static Length CreateLength(
    const InterpolableValue& interpolable_value,
    const CSSSizeNonInterpolableValue& non_interpolable_value,
    const CSSToLengthConversionData& conversion_data) {
  if (non_interpolable_value.IsKeyword()) {
    DCHECK_EQ(non_interpolable_value.Keyword(), CSSValueAuto);
    return Length(kAuto);
  }
  return LengthInterpolationFunctions::CreateLength(
      interpolable_value, non_interpolable_value.LengthNonInterpolableValue(),
      conversion_data, kValueRangeNonNegative);
}

FillSize SizeInterpolationFunctions::CreateFillSize(
    const InterpolableValue& interpolable_value_a,
    const NonInterpolableValue* non_interpolable_value_a,
    const InterpolableValue& interpolable_value_b,
    const NonInterpolableValue* non_interpolable_value_b,
    const CSSToLengthConversionData& conversion_data) {
  const auto& side_a = ToCSSSizeNonInterpolableValue(*non_interpolable_value_a);
  const auto& side_b = ToCSSSizeNonInterpolableValue(*non_interpolable_value_b);
  if (side_a.IsKeyword()) {
    switch (side_a.Keyword()) {
      case CSSValueCover:
        DCHECK_EQ(side_a.Keyword(), side_b.Keyword());
        return FillSize(kCover, LengthSize());
      case CSSValueContain:
        DCHECK_EQ(side_a.Keyword(), side_b.Keyword());
        return FillSize(kContain, LengthSize());
      case CSSValueAuto:
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  return FillSize(
      kSizeLength,
      LengthSize(CreateLength(interpolable_value_a, side_a, conversion_data),
                 CreateLength(interpolable_value_b, side_b, conversion_data)));
}

}  // namespace blink
