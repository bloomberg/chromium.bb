// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/stroke_width.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

const CSSValue* StrokeWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_property_parser_helpers::ConsumeLengthOrPercent(
      range, kSVGAttributeMode, kValueRangeNonNegative,
      css_property_parser_helpers::UnitlessQuirk::kForbid);
}

const CSSValue* StrokeWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  const Length& length = svg_style.StrokeWidth().length();
  if (length.IsFixed()) {
    return CSSPrimitiveValue::Create(length.Value(),
                                     CSSPrimitiveValue::UnitType::kPixels);
  }
  return CSSValue::Create(length, style.EffectiveZoom());
}

}  // namespace css_longhand
}  // namespace blink
