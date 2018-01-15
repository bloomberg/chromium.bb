// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/Contain.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

// none | strict | content | [ layout || style || paint || size ]
const CSSValue* Contain::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (id == CSSValueStrict || id == CSSValueContent) {
    list->Append(*CSSPropertyParserHelpers::ConsumeIdent(range));
    return list;
  }
  while (true) {
    CSSIdentifierValue* ident = CSSPropertyParserHelpers::ConsumeIdent<
        CSSValuePaint, CSSValueLayout, CSSValueStyle, CSSValueSize>(range);
    if (!ident)
      break;
    if (list->HasValue(*ident))
      return nullptr;
    list->Append(*ident);
  }

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
  if (style.ContainsStyle())
    list->Append(*CSSIdentifierValue::Create(CSSValueStyle));
  if (style.Contain() & kContainsLayout)
    list->Append(*CSSIdentifierValue::Create(CSSValueLayout));
  if (style.ContainsPaint())
    list->Append(*CSSIdentifierValue::Create(CSSValuePaint));
  if (style.ContainsSize())
    list->Append(*CSSIdentifierValue::Create(CSSValueSize));
  DCHECK(list->length());
  return list;
}

}  // namespace CSSLonghand
}  // namespace blink
