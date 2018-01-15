// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/Perspective.h"

#include "core/css/ZoomAdjustedPixelValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/frame/UseCounter.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* Perspective::ParseSingleValue(
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

const CSSValue* Perspective::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  if (!style.HasPerspective())
    return CSSIdentifierValue::Create(CSSValueNone);
  return ZoomAdjustedPixelValue(style.Perspective(), style);
}

}  // namespace CSSLonghand
}  // namespace blink
