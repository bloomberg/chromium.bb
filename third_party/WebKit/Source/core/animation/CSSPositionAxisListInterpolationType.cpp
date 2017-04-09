// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSPositionAxisListInterpolationType.h"

#include "core/animation/LengthInterpolationFunctions.h"
#include "core/animation/ListInterpolationFunctions.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"

namespace blink {

InterpolationValue
CSSPositionAxisListInterpolationType::ConvertPositionAxisCSSValue(
    const CSSValue& value) {
  if (value.IsValuePair()) {
    const CSSValuePair& pair = ToCSSValuePair(value);
    InterpolationValue result =
        LengthInterpolationFunctions::MaybeConvertCSSValue(pair.Second());
    CSSValueID side = ToCSSIdentifierValue(pair.First()).GetValueID();
    if (side == CSSValueRight || side == CSSValueBottom)
      LengthInterpolationFunctions::SubtractFromOneHundredPercent(result);
    return result;
  }

  if (value.IsPrimitiveValue())
    return LengthInterpolationFunctions::MaybeConvertCSSValue(value);

  if (!value.IsIdentifierValue())
    return nullptr;

  const CSSIdentifierValue& ident = ToCSSIdentifierValue(value);
  switch (ident.GetValueID()) {
    case CSSValueLeft:
    case CSSValueTop:
      return LengthInterpolationFunctions::CreateInterpolablePercent(0);
    case CSSValueRight:
    case CSSValueBottom:
      return LengthInterpolationFunctions::CreateInterpolablePercent(100);
    case CSSValueCenter:
      return LengthInterpolationFunctions::CreateInterpolablePercent(50);
    default:
      NOTREACHED();
      return nullptr;
  }
}

InterpolationValue CSSPositionAxisListInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.IsBaseValueList()) {
    return ListInterpolationFunctions::CreateList(
        1, [&value](size_t) { return ConvertPositionAxisCSSValue(value); });
  }

  const CSSValueList& list = ToCSSValueList(value);
  return ListInterpolationFunctions::CreateList(
      list.length(), [&list](size_t index) {
        return ConvertPositionAxisCSSValue(list.Item(index));
      });
}

}  // namespace blink
