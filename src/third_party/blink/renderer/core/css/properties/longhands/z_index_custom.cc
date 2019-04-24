// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/z_index.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

const CSSValue* ZIndex::ParseSingleValue(CSSParserTokenRange& range,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueAuto)
    return css_property_parser_helpers::ConsumeIdent(range);
  return css_property_parser_helpers::ConsumeInteger(range);
}

const CSSValue* ZIndex::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  if (style.HasAutoZIndex() || !style.IsStackingContext())
    return CSSIdentifierValue::Create(CSSValueAuto);
  return CSSPrimitiveValue::Create(style.ZIndex(),
                                   CSSPrimitiveValue::UnitType::kInteger);
}

}  // namespace css_longhand
}  // namespace blink
