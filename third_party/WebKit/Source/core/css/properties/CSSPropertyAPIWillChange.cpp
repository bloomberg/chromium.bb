// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWillChange.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIWillChange::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  if (range.peek().id() == CSSValueAuto)
    return CSSPropertyParserHelpers::consumeIdent(range);

  CSSValueList* values = CSSValueList::createCommaSeparated();
  // Every comma-separated list of identifiers is a valid will-change value,
  // unless the list includes an explicitly disallowed identifier.
  while (true) {
    if (range.peek().type() != IdentToken)
      return nullptr;
    CSSPropertyID unresolvedProperty =
        unresolvedCSSPropertyID(range.peek().value());
    if (unresolvedProperty != CSSPropertyInvalid &&
        unresolvedProperty != CSSPropertyVariable) {
      DCHECK(CSSPropertyMetadata::isEnabledProperty(unresolvedProperty));
      // Now "all" is used by both CSSValue and CSSPropertyValue.
      // Need to return nullptr when currentValue is CSSPropertyAll.
      if (unresolvedProperty == CSSPropertyWillChange ||
          unresolvedProperty == CSSPropertyAll)
        return nullptr;
      values->append(*CSSCustomIdentValue::create(unresolvedProperty));
      range.consumeIncludingWhitespace();
    } else {
      switch (range.peek().id()) {
        case CSSValueNone:
        case CSSValueAll:
        case CSSValueAuto:
        case CSSValueDefault:
        case CSSValueInitial:
        case CSSValueInherit:
          return nullptr;
        case CSSValueContents:
        case CSSValueScrollPosition:
          values->append(*CSSPropertyParserHelpers::consumeIdent(range));
          break;
        default:
          range.consumeIncludingWhitespace();
          break;
      }
    }

    if (range.atEnd())
      break;
    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range))
      return nullptr;
  }

  return values;
}

}  // namespace blink
