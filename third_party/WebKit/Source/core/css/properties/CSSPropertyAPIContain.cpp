// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIContain.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

// none | strict | content | [ layout || style || paint || size ]
const CSSValue* CSSPropertyAPIContain::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSValueID id = range.peek().id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::consumeIdent(range);

  CSSValueList* list = CSSValueList::createSpaceSeparated();
  if (id == CSSValueStrict || id == CSSValueContent) {
    list->append(*CSSPropertyParserHelpers::consumeIdent(range));
    return list;
  }
  while (true) {
    CSSIdentifierValue* ident = CSSPropertyParserHelpers::consumeIdent<
        CSSValuePaint, CSSValueLayout, CSSValueStyle, CSSValueSize>(range);
    if (!ident)
      break;
    if (list->hasValue(*ident))
      return nullptr;
    list->append(*ident);
  }

  if (!list->length())
    return nullptr;
  return list;
}

}  // namespace blink
