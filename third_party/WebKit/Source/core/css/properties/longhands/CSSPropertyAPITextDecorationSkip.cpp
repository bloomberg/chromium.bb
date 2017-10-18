// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CSSPropertyAPITextDecorationSkip.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPITextDecorationSkip::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  while (true) {
    CSSIdentifierValue* ident =
        CSSPropertyParserHelpers::ConsumeIdent<CSSValueObjects, CSSValueInk>(
            range);
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
