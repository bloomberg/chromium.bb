// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/GridGap.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSShorthand {

bool GridGap::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  DCHECK_EQ(shorthandForProperty(CSSPropertyGridGap).length(), 2u);
  CSSValue* row_gap = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative);
  CSSValue* column_gap = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative);
  if (!row_gap || !range.AtEnd())
    return false;
  if (!column_gap)
    column_gap = row_gap;
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridRowGap, CSSPropertyGridGap, *row_gap, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridColumnGap, CSSPropertyGridGap, *column_gap, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* GridGap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      gridGapShorthand(), style, layout_object, styled_node,
      allow_visited_style);
}

}  // namespace CSSShorthand
}  // namespace blink
