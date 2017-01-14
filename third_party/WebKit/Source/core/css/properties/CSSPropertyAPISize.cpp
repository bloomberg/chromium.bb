// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPISize.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

static CSSValue* consumePageSize(CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::consumeIdent<
      CSSValueA3, CSSValueA4, CSSValueA5, CSSValueB4, CSSValueB5,
      CSSValueLedger, CSSValueLegal, CSSValueLetter>(range);
}

const CSSValue* CSSPropertyAPISize::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSValueList* result = CSSValueList::createSpaceSeparated();

  if (range.peek().id() == CSSValueAuto) {
    result->append(*CSSPropertyParserHelpers::consumeIdent(range));
    return result;
  }

  if (CSSValue* width = CSSPropertyParserHelpers::consumeLength(
          range, context->mode(), ValueRangeNonNegative)) {
    CSSValue* height = CSSPropertyParserHelpers::consumeLength(
        range, context->mode(), ValueRangeNonNegative);
    result->append(*width);
    if (height)
      result->append(*height);
    return result;
  }

  CSSValue* pageSize = consumePageSize(range);
  CSSValue* orientation =
      CSSPropertyParserHelpers::consumeIdent<CSSValuePortrait,
                                             CSSValueLandscape>(range);
  if (!pageSize)
    pageSize = consumePageSize(range);

  if (!orientation && !pageSize)
    return nullptr;
  if (pageSize)
    result->append(*pageSize);
  if (orientation)
    result->append(*orientation);
  return result;
}

}  // namespace blink
