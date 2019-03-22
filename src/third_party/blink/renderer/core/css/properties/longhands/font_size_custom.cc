// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/font_size.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

const CSSValue* FontSize::ParseSingleValue(CSSParserTokenRange& range,
                                           const CSSParserContext& context,
                                           const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeFontSize(
      range, context.Mode(),
      css_property_parser_helpers::UnitlessQuirk::kAllow);
}

const CSSValue* FontSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontSize(style);
}

}  // namespace css_longhand
}  // namespace blink
