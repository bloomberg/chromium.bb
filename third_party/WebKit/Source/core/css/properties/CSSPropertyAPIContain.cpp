// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIContain.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

// none | strict | content | [ layout || style || paint || size ]
const CSSValue* CSSPropertyAPIContain::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
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

}  // namespace blink
