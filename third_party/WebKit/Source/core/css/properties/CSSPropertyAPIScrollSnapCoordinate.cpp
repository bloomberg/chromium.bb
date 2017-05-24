// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIScrollSnapCoordinate.h"

#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

// TODO(crbug.com/724912): Retire scroll-snap-coordinate

namespace blink {

using namespace CSSPropertyParserHelpers;

static CSSValueList* ConsumePositionList(CSSParserTokenRange& range,
                                         const CSSParserContext& context) {
  CSSValueList* positions = CSSValueList::CreateCommaSeparated();
  do {
    CSSValue* position = ConsumePosition(range, context, UnitlessQuirk::kForbid,
                                         Optional<UseCounter::Feature>());
    if (!position)
      return nullptr;
    positions->Append(*position);
  } while (ConsumeCommaIncludingWhitespace(range));
  return positions;
}

const CSSValue* CSSPropertyAPIScrollSnapCoordinate::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPropertyID) {
  if (range.Peek().Id() == CSSValueNone)
    return ConsumeIdent(range);
  return ConsumePositionList(range, context);
}

}  // namespace blink
