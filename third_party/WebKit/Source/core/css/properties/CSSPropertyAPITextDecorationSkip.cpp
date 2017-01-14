// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPITextDecorationSkip.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

const CSSValue* CSSPropertyAPITextDecorationSkip::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  DCHECK(RuntimeEnabledFeatures::css3TextDecorationsEnabled());
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  while (true) {
    CSSIdentifierValue* ident =
        CSSPropertyParserHelpers::consumeIdent<CSSValueObjects, CSSValueInk>(
            range);
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
