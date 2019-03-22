// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/shorthands/columns.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {
namespace css_shorthand {

bool Columns::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  CSSValue* column_width = nullptr;
  CSSValue* column_count = nullptr;
  if (!css_parsing_utils::ConsumeColumnWidthOrCount(range, column_width,
                                                    column_count))
    return false;
  css_parsing_utils::ConsumeColumnWidthOrCount(range, column_width,
                                               column_count);
  if (!range.AtEnd())
    return false;
  if (!column_width)
    column_width = CSSIdentifierValue::Create(CSSValueAuto);
  if (!column_count)
    column_count = CSSIdentifierValue::Create(CSSValueAuto);
  css_property_parser_helpers::AddProperty(
      CSSPropertyColumnWidth, CSSPropertyInvalid, *column_width, important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyColumnCount, CSSPropertyInvalid, *column_count, important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  return true;
}

const CSSValue* Columns::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValuesForShorthandProperty(
      columnsShorthand(), style, layout_object, styled_node,
      allow_visited_style);
}

}  // namespace css_shorthand
}  // namespace blink
