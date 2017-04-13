// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIScrollSnapCoordinate.h"

#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

static CSSValueList* ConsumePositionList(CSSParserTokenRange& range,
                                         CSSParserMode css_parser_mode) {
  CSSValueList* positions = CSSValueList::CreateCommaSeparated();
  do {
    CSSValue* position =
        ConsumePosition(range, css_parser_mode,
                        CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
    if (!position)
      return nullptr;
    positions->Append(*position);
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));
  return positions;
}

const CSSValue* CSSPropertyAPIScrollSnapCoordinate::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPropertyID) {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return ConsumePositionList(range, context.Mode());
}

}  // namespace blink
