// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/text_underline_position.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

// auto | [ under || [ left | right ] ]
const CSSValue* TextUnderlinePosition::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_property_parser_helpers::ConsumeIdent(range);

  CSSIdentifierValue* under_value =
      css_property_parser_helpers::ConsumeIdent<CSSValueID::kUnder>(range);
  CSSIdentifierValue* left_or_right_value = nullptr;
  if (RuntimeEnabledFeatures::TextUnderlinePositionLeftRightEnabled()) {
    left_or_right_value =
        css_property_parser_helpers::ConsumeIdent<CSSValueID::kLeft,
                                                  CSSValueID::kRight>(range);
    if (left_or_right_value && !under_value) {
      under_value =
          css_property_parser_helpers::ConsumeIdent<CSSValueID::kUnder>(range);
    }
  }
  if (!under_value && !left_or_right_value) {
    return nullptr;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (under_value)
    list->Append(*under_value);
  if (left_or_right_value)
    list->Append(*left_or_right_value);
  return list;
}

const CSSValue* TextUnderlinePosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  auto text_underline_position = style.TextUnderlinePosition();
  if (text_underline_position == kTextUnderlinePositionAuto)
    return CSSIdentifierValue::Create(CSSValueID::kAuto);
  if (text_underline_position == kTextUnderlinePositionUnder)
    return CSSIdentifierValue::Create(CSSValueID::kUnder);
  if (text_underline_position == kTextUnderlinePositionLeft)
    return CSSIdentifierValue::Create(CSSValueID::kLeft);
  if (text_underline_position == kTextUnderlinePositionRight)
    return CSSIdentifierValue::Create(CSSValueID::kRight);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  DCHECK(text_underline_position & kTextUnderlinePositionUnder);
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kUnder));
  if (text_underline_position & kTextUnderlinePositionLeft)
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kLeft));
  if (text_underline_position & kTextUnderlinePositionRight)
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kRight));
  DCHECK_EQ(list->length(), 2U);
  return list;
}

}  // namespace css_longhand
}  // namespace blink
