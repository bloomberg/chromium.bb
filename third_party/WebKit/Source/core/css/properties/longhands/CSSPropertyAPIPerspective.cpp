// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CSSPropertyAPIPerspective.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/frame/UseCounter.h"

namespace blink {

const CSSValue* CSSPropertyAPIPerspective::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& localContext) const {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  CSSPrimitiveValue* parsed_value = CSSPropertyParserHelpers::ConsumeLength(
      range, context.Mode(), kValueRangeAll);
  bool use_legacy_parsing = localContext.UseAliasParsing();
  if (!parsed_value && use_legacy_parsing) {
    double perspective;
    if (!CSSPropertyParserHelpers::ConsumeNumberRaw(range, perspective))
      return nullptr;
    context.Count(WebFeature::kUnitlessPerspectiveInPerspectiveProperty);
    parsed_value = CSSPrimitiveValue::Create(
        perspective, CSSPrimitiveValue::UnitType::kPixels);
  }
  if (parsed_value &&
      (parsed_value->IsCalculated() || parsed_value->GetDoubleValue() > 0))
    return parsed_value;
  return nullptr;
}

}  // namespace blink
