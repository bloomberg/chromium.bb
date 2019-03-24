// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/contain.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

// none | strict | content | [ size || layout || style || paint ]
const CSSValue* Contain::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone)
    return css_property_parser_helpers::ConsumeIdent(range);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (id == CSSValueID::kStrict || id == CSSValueID::kContent) {
    list->Append(*css_property_parser_helpers::ConsumeIdent(range));
    return list;
  }

  CSSIdentifierValue* size = nullptr;
  CSSIdentifierValue* layout = nullptr;
  CSSIdentifierValue* style = nullptr;
  CSSIdentifierValue* paint = nullptr;
  while (true) {
    id = range.Peek().Id();
    if (id == CSSValueID::kSize && !size)
      size = css_property_parser_helpers::ConsumeIdent(range);
    else if (id == CSSValueID::kLayout && !layout)
      layout = css_property_parser_helpers::ConsumeIdent(range);
    else if (id == CSSValueID::kStyle && !style)
      style = css_property_parser_helpers::ConsumeIdent(range);
    else if (id == CSSValueID::kPaint && !paint)
      paint = css_property_parser_helpers::ConsumeIdent(range);
    else
      break;
  }
  if (size)
    list->Append(*size);
  if (layout)
    list->Append(*layout);
  if (style)
    list->Append(*style);
  if (paint)
    list->Append(*paint);
  if (!list->length())
    return nullptr;
  return list;
}

const CSSValue* Contain::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  if (!style.Contain())
    return CSSIdentifierValue::Create(CSSValueID::kNone);
  if (style.Contain() == kContainsStrict)
    return CSSIdentifierValue::Create(CSSValueID::kStrict);
  if (style.Contain() == kContainsContent)
    return CSSIdentifierValue::Create(CSSValueID::kContent);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (style.ContainsSize())
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kSize));
  if (style.Contain() & kContainsLayout)
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kLayout));
  if (style.ContainsStyle())
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kStyle));
  if (style.ContainsPaint())
    list->Append(*CSSIdentifierValue::Create(CSSValueID::kPaint));
  DCHECK(list->length());
  return list;
}

}  // namespace css_longhand
}  // namespace blink
