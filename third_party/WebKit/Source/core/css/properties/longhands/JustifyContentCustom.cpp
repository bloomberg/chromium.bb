// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/JustifyContent.h"

#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* JustifyContent::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // justify-content property does not allow the <baseline-position> values.
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueFirst, CSSValueLast,
                                             CSSValueBaseline>(
          range.Peek().Id()))
    return nullptr;
  return CSSParsingUtils::ConsumeContentDistributionOverflowPosition(
      range, CSSParsingUtils::IsContentPositionOrLeftOrRightKeyword);
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

}  // namespace CSSLonghand
}  // namespace blink
