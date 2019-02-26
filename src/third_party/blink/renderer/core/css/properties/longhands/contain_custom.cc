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
  if (id == CSSValueNone)
    return css_property_parser_helpers::ConsumeIdent(range);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (id == CSSValueStrict || id == CSSValueContent) {
    list->Append(*css_property_parser_helpers::ConsumeIdent(range));
    return list;
  }

  CSSIdentifierValue* size = nullptr;
  CSSIdentifierValue* layout = nullptr;
  CSSIdentifierValue* style = nullptr;
  CSSIdentifierValue* paint = nullptr;
  while (true) {
    CSSValueID id = range.Peek().Id();
    if (id == CSSValueSize && !size)
      size = css_property_parser_helpers::ConsumeIdent(range);
    else if (id == CSSValueLayout && !layout)
      layout = css_property_parser_helpers::ConsumeIdent(range);
    else if (id == CSSValueStyle && !style)
      style = css_property_parser_helpers::ConsumeIdent(range);
    else if (id == CSSValuePaint && !paint)
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
    return CSSIdentifierValue::Create(CSSValueNone);
  if (style.Contain() == kContainsStrict)
    return CSSIdentifierValue::Create(CSSValueStrict);
  if (style.Contain() == kContainsContent)
    return CSSIdentifierValue::Create(CSSValueContent);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (style.ContainsSize())
    list->Append(*CSSIdentifierValue::Create(CSSValueSize));
  if (style.Contain() & kContainsLayout)
    list->Append(*CSSIdentifierValue::Create(CSSValueLayout));
  if (style.ContainsStyle())
    list->Append(*CSSIdentifierValue::Create(CSSValueStyle));
  if (style.ContainsPaint())
    list->Append(*CSSIdentifierValue::Create(CSSValuePaint));
  DCHECK(list->length());
  return list;
}

}  // namespace css_longhand
}  // namespace blink
