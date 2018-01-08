// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/TabSize.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* TabSize::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  CSSPrimitiveValue* parsed_value =
      CSSPropertyParserHelpers::ConsumeInteger(range, 0);
  if (parsed_value)
    return parsed_value;
  return CSSPropertyParserHelpers::ConsumeLength(range, context.Mode(),
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

}  // namespace CSSLonghand
}  // namespace blink
