// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWillChange.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIWillChange::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueAuto)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValueList* values = CSSValueList::CreateCommaSeparated();
  // Every comma-separated list of identifiers is a valid will-change value,
  // unless the list includes an explicitly disallowed identifier.
  while (true) {
    if (range.Peek().GetType() != kIdentToken)
      return nullptr;
    CSSPropertyID unresolved_property =
        UnresolvedCSSPropertyID(range.Peek().Value());
    if (unresolved_property != CSSPropertyInvalid &&
        unresolved_property != CSSPropertyVariable) {
      DCHECK(CSSPropertyAPI::Get(resolveCSSPropertyID(unresolved_property))
                 .IsEnabled());
      // Now "all" is used by both CSSValue and CSSPropertyValue.
      // Need to return nullptr when currentValue is CSSPropertyAll.
      if (unresolved_property == CSSPropertyWillChange ||
          unresolved_property == CSSPropertyAll)
        return nullptr;
      values->Append(*CSSCustomIdentValue::Create(unresolved_property));
      range.ConsumeIncludingWhitespace();
    } else {
      switch (range.Peek().Id()) {
        case CSSValueNone:
        case CSSValueAll:
        case CSSValueAuto:
        case CSSValueDefault:
        case CSSValueInitial:
        case CSSValueInherit:
          return nullptr;
        case CSSValueContents:
        case CSSValueScrollPosition:
          values->Append(*CSSPropertyParserHelpers::ConsumeIdent(range));
          break;
        default:
          range.ConsumeIncludingWhitespace();
          break;
      }
    }

    if (range.AtEnd())
      break;
    if (!CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range))
      return nullptr;
  }

  return values;
}

}  // namespace blink
