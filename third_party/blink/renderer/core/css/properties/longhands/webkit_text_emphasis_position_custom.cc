// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/webkit_text_emphasis_position.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

class CSSParserLocalContext;

namespace css_longhand {

// [ over | under ] && [ right | left ]?
// If [ right | left ] is omitted, it defaults to right.
const CSSValue* WebkitTextEmphasisPosition::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSIdentifierValue* values[2] = {
      css_property_parser_helpers::ConsumeIdent<
          CSSValueID::kOver, CSSValueID::kUnder, CSSValueID::kRight,
          CSSValueID::kLeft>(range),
      nullptr};
  if (!values[0])
    return nullptr;
  values[1] = css_property_parser_helpers::ConsumeIdent<
      CSSValueID::kOver, CSSValueID::kUnder, CSSValueID::kRight,
      CSSValueID::kLeft>(range);
  CSSIdentifierValue* over_under = nullptr;
  CSSIdentifierValue* left_right = nullptr;

  for (auto* value : values) {
    if (!value)
      break;
    switch (value->GetValueID()) {
      case CSSValueID::kOver:
      case CSSValueID::kUnder:
        if (over_under)
          return nullptr;
        over_under = value;
        break;
      case CSSValueID::kLeft:
      case CSSValueID::kRight:
        if (left_right)
          return nullptr;
        left_right = value;
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  if (!over_under)
    return nullptr;
  if (!left_right)
    left_right = CSSIdentifierValue::Create(CSSValueID::kRight);
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*over_under);
  list->Append(*left_right);
  return list;
}

const CSSValue* WebkitTextEmphasisPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  switch (style.GetTextEmphasisPosition()) {
    case TextEmphasisPosition::kOverRight:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kOver));
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kRight));
      break;
    case TextEmphasisPosition::kOverLeft:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kOver));
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kLeft));
      break;
    case TextEmphasisPosition::kUnderRight:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kUnder));
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kRight));
      break;
    case TextEmphasisPosition::kUnderLeft:
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kUnder));
      list->Append(*CSSIdentifierValue::Create(CSSValueID::kLeft));
      break;
  }
  return list;
}

}  // namespace css_longhand
}  // namespace blink
