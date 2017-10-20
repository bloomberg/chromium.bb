// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/ShadowInterpolationFunctions.h"

#include "core/animation/CSSColorInterpolationType.h"
#include "core/animation/InterpolationValue.h"
#include "core/animation/LengthInterpolationFunctions.h"
#include "core/animation/NonInterpolableValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/ShadowData.h"
#include "platform/geometry/FloatPoint.h"
#include <memory>

namespace blink {

enum ShadowComponentIndex : unsigned {
  kShadowX,
  kShadowY,
  kShadowBlur,
  kShadowSpread,
  kShadowColor,
  kShadowComponentIndexCount,
};

class ShadowNonInterpolableValue : public NonInterpolableValue {
 public:
  ~ShadowNonInterpolableValue() final {}

  static scoped_refptr<ShadowNonInterpolableValue> Create(
      ShadowStyle shadow_style) {
    return WTF::AdoptRef(new ShadowNonInterpolableValue(shadow_style));
  }

  ShadowStyle Style() const { return style_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  ShadowNonInterpolableValue(ShadowStyle shadow_style) : style_(shadow_style) {}

  ShadowStyle style_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(ShadowNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(ShadowNonInterpolableValue);

bool ShadowInterpolationFunctions::NonInterpolableValuesAreCompatible(
    const NonInterpolableValue* a,
    const NonInterpolableValue* b) {
  return ToShadowNonInterpolableValue(*a).Style() ==
         ToShadowNonInterpolableValue(*b).Style();
}

PairwiseInterpolationValue ShadowInterpolationFunctions::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) {
  if (!NonInterpolableValuesAreCompatible(start.non_interpolable_value.get(),
                                          end.non_interpolable_value.get()))
    return nullptr;
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

InterpolationValue ShadowInterpolationFunctions::ConvertShadowData(
    const ShadowData& shadow_data,
    double zoom) {
  std::unique_ptr<InterpolableList> interpolable_list =
      InterpolableList::Create(kShadowComponentIndexCount);
  interpolable_list->Set(kShadowX,
                         LengthInterpolationFunctions::CreateInterpolablePixels(
                             shadow_data.X() / zoom));
  interpolable_list->Set(kShadowY,
                         LengthInterpolationFunctions::CreateInterpolablePixels(
                             shadow_data.Y() / zoom));
  interpolable_list->Set(kShadowBlur,
                         LengthInterpolationFunctions::CreateInterpolablePixels(
                             shadow_data.Blur() / zoom));
  interpolable_list->Set(kShadowSpread,
                         LengthInterpolationFunctions::CreateInterpolablePixels(
                             shadow_data.Spread() / zoom));
  interpolable_list->Set(kShadowColor,
                         CSSColorInterpolationType::CreateInterpolableColor(
                             shadow_data.GetColor()));
  return InterpolationValue(
      std::move(interpolable_list),
      ShadowNonInterpolableValue::Create(shadow_data.Style()));
}

InterpolationValue ShadowInterpolationFunctions::MaybeConvertCSSValue(
    const CSSValue& value) {
  if (!value.IsShadowValue())
    return nullptr;
  const CSSShadowValue& shadow = ToCSSShadowValue(value);

  ShadowStyle style = kNormal;
  if (shadow.style) {
    if (shadow.style->GetValueID() == CSSValueInset)
      style = kInset;
    else
      return nullptr;
  }

  std::unique_ptr<InterpolableList> interpolable_list =
      InterpolableList::Create(kShadowComponentIndexCount);
  static_assert(kShadowX == 0, "Enum ordering check.");
  static_assert(kShadowY == 1, "Enum ordering check.");
  static_assert(kShadowBlur == 2, "Enum ordering check.");
  static_assert(kShadowSpread == 3, "Enum ordering check.");
  const CSSPrimitiveValue* lengths[] = {
      shadow.x.Get(), shadow.y.Get(), shadow.blur.Get(), shadow.spread.Get(),
  };
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(lengths); i++) {
    if (lengths[i]) {
      InterpolationValue length_field =
          LengthInterpolationFunctions::MaybeConvertCSSValue(*lengths[i]);
      if (!length_field)
        return nullptr;
      DCHECK(!length_field.non_interpolable_value);
      interpolable_list->Set(i, std::move(length_field.interpolable_value));
    } else {
      interpolable_list->Set(
          i, LengthInterpolationFunctions::CreateInterpolablePixels(0));
    }
  }

  if (shadow.color) {
    std::unique_ptr<InterpolableValue> interpolable_color =
        CSSColorInterpolationType::MaybeCreateInterpolableColor(*shadow.color);
    if (!interpolable_color)
      return nullptr;
    interpolable_list->Set(kShadowColor, std::move(interpolable_color));
  } else {
    interpolable_list->Set(kShadowColor,
                           CSSColorInterpolationType::CreateInterpolableColor(
                               StyleColor::CurrentColor()));
  }

  return InterpolationValue(std::move(interpolable_list),
                            ShadowNonInterpolableValue::Create(style));
}

std::unique_ptr<InterpolableValue>
ShadowInterpolationFunctions::CreateNeutralInterpolableValue() {
  return ConvertShadowData(ShadowData::NeutralValue(), 1).interpolable_value;
}

void ShadowInterpolationFunctions::Composite(
    std::unique_ptr<InterpolableValue>& underlying_interpolable_value,
    scoped_refptr<NonInterpolableValue>& underlying_non_interpolable_value,
    double underlying_fraction,
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value) {
  DCHECK(NonInterpolableValuesAreCompatible(
      underlying_non_interpolable_value.get(), non_interpolable_value));
  InterpolableList& underlying_interpolable_list =
      ToInterpolableList(*underlying_interpolable_value);
  const InterpolableList& interpolable_list =
      ToInterpolableList(interpolable_value);
  underlying_interpolable_list.ScaleAndAdd(underlying_fraction,
                                           interpolable_list);
}

ShadowData ShadowInterpolationFunctions::CreateShadowData(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    const StyleResolverState& state) {
  const InterpolableList& interpolable_list =
      ToInterpolableList(interpolable_value);
  const ShadowNonInterpolableValue& shadow_non_interpolable_value =
      ToShadowNonInterpolableValue(*non_interpolable_value);
  const CSSToLengthConversionData& conversion_data =
      state.CssToLengthConversionData();
  Length shadow_x = LengthInterpolationFunctions::CreateLength(
      *interpolable_list.Get(kShadowX), nullptr, conversion_data,
      kValueRangeAll);
  Length shadow_y = LengthInterpolationFunctions::CreateLength(
      *interpolable_list.Get(kShadowY), nullptr, conversion_data,
      kValueRangeAll);
  Length shadow_blur = LengthInterpolationFunctions::CreateLength(
      *interpolable_list.Get(kShadowBlur), nullptr, conversion_data,
      kValueRangeNonNegative);
  Length shadow_spread = LengthInterpolationFunctions::CreateLength(
      *interpolable_list.Get(kShadowSpread), nullptr, conversion_data,
      kValueRangeAll);
  DCHECK(shadow_x.IsFixed() && shadow_y.IsFixed() && shadow_blur.IsFixed() &&
         shadow_spread.IsFixed());
  return ShadowData(FloatPoint(shadow_x.Value(), shadow_y.Value()),
                    shadow_blur.Value(), shadow_spread.Value(),
                    shadow_non_interpolable_value.Style(),
                    CSSColorInterpolationType::ResolveInterpolableColor(
                        *interpolable_list.Get(kShadowColor), state));
}

}  // namespace blink
