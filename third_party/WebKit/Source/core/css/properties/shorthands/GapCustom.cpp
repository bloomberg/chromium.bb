// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/gap.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style_property_shorthand.h"

namespace blink {
namespace CSSShorthand {

bool Gap::ParseShorthand(bool important,
                         CSSParserTokenRange& range,
                         const CSSParserContext& context,
                         const CSSParserLocalContext&,
                         HeapVector<CSSPropertyValue, 256>& properties) const {
  DCHECK_EQ(shorthandForProperty(CSSPropertyGap).length(), 2u);
  CSSValue* row_gap = CSSParsingUtils::ConsumeGapLength(range, context);
  CSSValue* column_gap = CSSParsingUtils::ConsumeGapLength(range, context);
  if (!row_gap || !range.AtEnd())
    return false;
  if (!column_gap)
    column_gap = row_gap;
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyRowGap, CSSPropertyGap, *row_gap, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyColumnGap, CSSPropertyGap, *column_gap, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* Gap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      gapShorthand(), style, layout_object, styled_node, allow_visited_style);
}

}  // namespace CSSShorthand
}  // namespace blink
