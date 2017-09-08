// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyTextDecorationLineUtils.h"

#include "core/CSSValueKeywords.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

class CSSIdentifierValue;

CSSValue* CSSPropertyTextDecorationLineUtils::ConsumeTextDecorationLine(
    CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  while (true) {
    CSSIdentifierValue* ident =
        CSSPropertyParserHelpers::ConsumeIdent<CSSValueBlink, CSSValueUnderline,
                                               CSSValueOverline,
                                               CSSValueLineThrough>(range);
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
