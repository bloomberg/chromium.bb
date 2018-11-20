// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/tab_size.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

const CSSValue* TabSize::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  CSSPrimitiveValue* parsed_value =
      css_property_parser_helpers::ConsumeInteger(range, 0);
  if (parsed_value)
    return parsed_value;
  return css_property_parser_helpers::ConsumeLength(range, context.Mode(),
                                                    kValueRangeNonNegative);
}

const CSSValue* TabSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return CSSPrimitiveValue::Create(style.GetTabSize().GetPixelSize(1.0),
                                   style.GetTabSize().IsSpaces()
                                       ? CSSPrimitiveValue::UnitType::kNumber
                                       : CSSPrimitiveValue::UnitType::kPixels);
}

}  // namespace css_longhand
}  // namespace blink
