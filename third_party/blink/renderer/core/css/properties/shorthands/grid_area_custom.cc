// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/shorthands/grid_area.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {
namespace css_shorthand {

bool GridArea::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  DCHECK_EQ(gridAreaShorthand().length(), 4u);

  CSSValue* row_start_value =
      css_parsing_utils::ConsumeGridLine(range, context);
  if (!row_start_value)
    return false;
  CSSValue* column_start_value = nullptr;
  CSSValue* row_end_value = nullptr;
  CSSValue* column_end_value = nullptr;
  if (css_property_parser_helpers::ConsumeSlashIncludingWhitespace(range)) {
    column_start_value = css_parsing_utils::ConsumeGridLine(range, context);
    if (!column_start_value)
      return false;
    if (css_property_parser_helpers::ConsumeSlashIncludingWhitespace(range)) {
      row_end_value = css_parsing_utils::ConsumeGridLine(range, context);
      if (!row_end_value)
        return false;
      if (css_property_parser_helpers::ConsumeSlashIncludingWhitespace(range)) {
        column_end_value = css_parsing_utils::ConsumeGridLine(range, context);
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

  css_property_parser_helpers::AddProperty(
      CSSPropertyGridRowStart, CSSPropertyGridArea, *row_start_value, important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyGridColumnStart, CSSPropertyGridArea, *column_start_value,
      important, css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyGridRowEnd, CSSPropertyGridArea, *row_end_value, important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyGridColumnEnd, CSSPropertyGridArea, *column_end_value,
      important, css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
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

}  // namespace css_shorthand
}  // namespace blink
