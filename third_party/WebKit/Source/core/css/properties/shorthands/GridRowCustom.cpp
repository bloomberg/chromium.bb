// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/GridRow.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSShorthand {

bool GridRow::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  const StylePropertyShorthand& shorthand =
      shorthandForProperty(CSSPropertyGridRow);
  DCHECK_EQ(shorthand.length(), 2u);

  CSSValue* start_value = nullptr;
  CSSValue* end_value = nullptr;
  if (!CSSParsingUtils::ConsumeGridItemPositionShorthand(
          important, range, start_value, end_value)) {
    return false;
  }

  CSSPropertyParserHelpers::AddProperty(
      shorthand.properties()[0]->PropertyID(), CSSPropertyGridRow, *start_value,
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      shorthand.properties()[1]->PropertyID(), CSSPropertyGridRow, *end_value,
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);

  return true;
}

const CSSValue* GridRow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForGridShorthand(gridRowShorthand(), style,
                                                    layout_object, styled_node,
                                                    allow_visited_style);
}

}  // namespace CSSShorthand
}  // namespace blink
