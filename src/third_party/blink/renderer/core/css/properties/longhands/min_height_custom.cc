// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/min_height.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

class CSSParserLocalContext;

namespace css_longhand {

const CSSValue* MinHeight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeWidthOrHeight(
      range, context, css_property_parser_helpers::UnitlessQuirk::kAllow);
}

const CSSValue* MinHeight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  if (style.MinHeight().IsAuto())
    return ComputedStyleUtils::MinWidthOrMinHeightAuto(styled_node, style);
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.MinHeight(),
                                                             style);
}

}  // namespace css_longhand
}  // namespace blink
