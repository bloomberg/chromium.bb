// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/GridArea.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSShorthand {

bool GridArea::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  DCHECK_EQ(gridAreaShorthand().length(), 4u);

  CSSValue* row_start_value = CSSParsingUtils::ConsumeGridLine(range);
  if (!row_start_value)
    return false;
  CSSValue* column_start_value = nullptr;
  CSSValue* row_end_value = nullptr;
  CSSValue* column_end_value = nullptr;
  if (CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range)) {
    column_start_value = CSSParsingUtils::ConsumeGridLine(range);
    if (!column_start_value)
      return false;
    if (CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range)) {
      row_end_value = CSSParsingUtils::ConsumeGridLine(range);
      if (!row_end_value)
        return false;
      if (CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range)) {
        column_end_value = CSSParsingUtils::ConsumeGridLine(range);
        if (!column_end_value)
          return false;
      }
    }
  }
  if (!range.AtEnd())
    return false;
  if (!column_start_value) {
    column_start_value = row_start_value->IsCustomIdentValue()
                             ? row_start_value
                             : CSSIdentifierValue::Create(CSSValueAuto);
  }
  if (!row_end_value) {
    row_end_value = row_start_value->IsCustomIdentValue()
                        ? row_start_value
                        : CSSIdentifierValue::Create(CSSValueAuto);
  }
  if (!column_end_value) {
    column_end_value = column_start_value->IsCustomIdentValue()
                           ? column_start_value
                           : CSSIdentifierValue::Create(CSSValueAuto);
  }

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridRowStart, CSSPropertyGridArea, *row_start_value, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridColumnStart, CSSPropertyGridArea, *column_start_value,
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridRowEnd, CSSPropertyGridArea, *row_end_value, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridColumnEnd, CSSPropertyGridArea, *column_end_value,
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  return true;
}

const CSSValue* GridArea::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForGridShorthand(gridAreaShorthand(), style,
                                                    layout_object, styled_node,
                                                    allow_visited_style);
}

}  // namespace CSSShorthand
}  // namespace blink
