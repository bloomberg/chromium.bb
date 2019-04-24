// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/justify_content.h"

#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

const CSSValue* JustifyContent::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // justify-content property does not allow the <baseline-position> values.
  if (css_property_parser_helpers::IdentMatches<CSSValueFirst, CSSValueLast,
                                                CSSValueBaseline>(
          range.Peek().Id()))
    return nullptr;
  return css_parsing_utils::ConsumeContentDistributionOverflowPosition(
      range, css_parsing_utils::IsContentPositionOrLeftOrRightKeyword);
}

const CSSValue* JustifyContent::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::
      ValueForContentPositionAndDistributionWithOverflowAlignment(
          style.JustifyContent());
}

}  // namespace css_longhand
}  // namespace blink
