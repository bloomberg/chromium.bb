// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/shorthands/place_self.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {
namespace CSSShorthand {

bool PlaceSelf::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  DCHECK_EQ(shorthandForProperty(CSSPropertyPlaceSelf).length(), 2u);

  CSSValue* align_self_value = nullptr;
  CSSValue* justify_self_value = nullptr;
  if (!CSSParsingUtils::ConsumePlaceAlignment(
          range, CSSParsingUtils::ConsumeSimplifiedSelfPosition,
          align_self_value, justify_self_value)) {
    return false;
  }

  DCHECK(align_self_value);
  DCHECK(justify_self_value);

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyAlignSelf, CSSPropertyPlaceSelf, *align_self_value, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyJustifySelf, CSSPropertyPlaceSelf, *justify_self_value,
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);

  return true;
}

const CSSValue* PlaceSelf::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  // TODO (jfernandez): The spec states that we should return the specified
  // value.
  return ComputedStyleUtils::ValuesForShorthandProperty(
      placeSelfShorthand(), style, layout_object, styled_node,
      allow_visited_style);
}

}  // namespace CSSShorthand
}  // namespace blink
