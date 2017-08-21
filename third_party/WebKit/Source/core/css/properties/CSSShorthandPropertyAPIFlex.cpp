// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIFlex.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

bool CSSShorthandPropertyAPIFlex::ParseShorthand(
    CSSPropertyID,
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSProperty, 256>& properties) const {
  static const double kUnsetValue = -1;
  double flex_grow = kUnsetValue;
  double flex_shrink = kUnsetValue;
  CSSValue* flex_basis = nullptr;

  if (range.Peek().Id() == CSSValueNone) {
    flex_grow = 0;
    flex_shrink = 0;
    flex_basis = CSSIdentifierValue::Create(CSSValueAuto);
    range.ConsumeIncludingWhitespace();
  } else {
    unsigned index = 0;
    while (!range.AtEnd() && index++ < 3) {
      double num;
      if (CSSPropertyParserHelpers::ConsumeNumberRaw(range, num)) {
        if (num < 0)
          return false;
        if (flex_grow == kUnsetValue) {
          flex_grow = num;
        } else if (flex_shrink == kUnsetValue) {
          flex_shrink = num;
        } else if (!num) {
          // flex only allows a basis of 0 (sans units) if
          // flex-grow and flex-shrink values have already been
          // set.
          flex_basis = CSSPrimitiveValue::Create(
              0, CSSPrimitiveValue::UnitType::kPixels);
        } else {
          return false;
        }
      } else if (!flex_basis) {
        if (range.Peek().Id() == CSSValueAuto)
          flex_basis = CSSPropertyParserHelpers::ConsumeIdent(range);
        if (!flex_basis) {
          flex_basis = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
              range, context.Mode(), kValueRangeNonNegative);
        }
        if (index == 2 && !range.AtEnd())
          return false;
      }
    }
    if (index == 0)
      return false;
    if (flex_grow == kUnsetValue)
      flex_grow = 1;
    if (flex_shrink == kUnsetValue)
      flex_shrink = 1;
    if (!flex_basis) {
      flex_basis = CSSPrimitiveValue::Create(
          0, CSSPrimitiveValue::UnitType::kPercentage);
    }
  }

  if (!range.AtEnd())
    return false;
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFlexGrow, CSSPropertyFlex,
      *CSSPrimitiveValue::Create(clampTo<float>(flex_grow),
                                 CSSPrimitiveValue::UnitType::kNumber),
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFlexShrink, CSSPropertyFlex,
      *CSSPrimitiveValue::Create(clampTo<float>(flex_shrink),
                                 CSSPrimitiveValue::UnitType::kNumber),
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFlexBasis, CSSPropertyFlex, *flex_basis, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);

  return true;
}

}  // namespace blink
