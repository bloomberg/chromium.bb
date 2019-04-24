// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/text_size_adjust.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

const CSSValue* TextSizeAdjust::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueAuto)
    return css_property_parser_helpers::ConsumeIdent(range);
  if (range.Peek().Id() == CSSValueNone)
    return css_property_parser_helpers::ConsumeIdent(range);
  return css_property_parser_helpers::ConsumePercent(range,
                                                     kValueRangeNonNegative);
}

const CSSValue* TextSizeAdjust::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  if (style.GetTextSizeAdjust().IsAuto())
    return CSSIdentifierValue::Create(CSSValueAuto);
  return CSSPrimitiveValue::Create(style.GetTextSizeAdjust().Multiplier() * 100,
                                   CSSPrimitiveValue::UnitType::kPercentage);
}

}  // namespace css_longhand
}  // namespace blink
