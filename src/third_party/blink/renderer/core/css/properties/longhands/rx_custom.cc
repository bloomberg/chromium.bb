// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/rx.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"

namespace blink {
namespace css_longhand {

const CSSValue* Rx::ParseSingleValue(CSSParserTokenRange& range,
                                     const CSSParserContext& context,
                                     const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueAuto)
    return css_property_parser_helpers::ConsumeIdent(range);
  return css_property_parser_helpers::ConsumeSVGGeometryPropertyLength(
      range, context, kValueRangeNonNegative);
}

const CSSValue* Rx::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(svg_style.Rx(),
                                                             style);
}

}  // namespace css_longhand
}  // namespace blink
