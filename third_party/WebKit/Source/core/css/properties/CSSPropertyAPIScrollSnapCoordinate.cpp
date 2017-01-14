// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIScrollSnapCoordinate.h"

#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

static CSSValueList* consumePositionList(CSSParserTokenRange& range,
                                         CSSParserMode cssParserMode) {
  CSSValueList* positions = CSSValueList::createCommaSeparated();
  do {
    CSSValue* position = consumePosition(
        range, cssParserMode, CSSPropertyParserHelpers::UnitlessQuirk::Forbid);
    if (!position)
      return nullptr;
    positions->append(*position);
  } while (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range));
  return positions;
}

const CSSValue* CSSPropertyAPIScrollSnapCoordinate::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  if (range.peek().id() == CSSValueNone)
    return CSSPropertyParserHelpers::consumeIdent(range);
  return consumePositionList(range, context->mode());
}

}  // namespace blink
