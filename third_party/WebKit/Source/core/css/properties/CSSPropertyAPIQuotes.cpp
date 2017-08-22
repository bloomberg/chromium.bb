// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIQuotes.h"

#include "core/css/CSSStringValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIQuotes::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  while (!range.AtEnd()) {
    CSSStringValue* parsed_value =
        CSSPropertyParserHelpers::ConsumeString(range);
    if (!parsed_value)
      return nullptr;
    values->Append(*parsed_value);
  }
  if (values->length() && values->length() % 2 == 0)
    return values;
  return nullptr;
}

}  // namespace blink
