// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/shorthands/border.h"

#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_shorthand {

bool Border::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  const CSSValue* width = nullptr;
  const CSSValue* style = nullptr;
  const CSSValue* color = nullptr;

  if (!css_property_parser_helpers::ConsumeBorderShorthand(
          range, context, width, style, color)) {
    return false;
  };

  css_property_parser_helpers::AddExpandedPropertyForValue(
      CSSPropertyBorderWidth, *width, important, properties);
  css_property_parser_helpers::AddExpandedPropertyForValue(
      CSSPropertyBorderStyle, *style, important, properties);
  css_property_parser_helpers::AddExpandedPropertyForValue(
      CSSPropertyBorderColor, *color, important, properties);
  css_property_parser_helpers::AddExpandedPropertyForValue(
      CSSPropertyBorderImage, *CSSInitialValue::Create(), important,
      properties);

  return range.AtEnd();
}

const CSSValue* Border::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  const CSSValue* value = GetCSSPropertyBorderTop().CSSValueFromComputedStyle(
      style, layout_object, styled_node, allow_visited_style);
  static const CSSProperty* kProperties[3] = {&GetCSSPropertyBorderRight(),
                                              &GetCSSPropertyBorderBottom(),
                                              &GetCSSPropertyBorderLeft()};
  for (size_t i = 0; i < arraysize(kProperties); ++i) {
    const CSSValue* value_for_side = kProperties[i]->CSSValueFromComputedStyle(
        style, layout_object, styled_node, allow_visited_style);
    if (!DataEquivalent(value, value_for_side)) {
      return nullptr;
    }
  }
  return value;
}

}  // namespace css_shorthand
}  // namespace blink
