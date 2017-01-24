// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIFontVariantNumeric.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/parser/FontVariantNumericParser.h"

namespace blink {

const CSSValue* CSSPropertyAPIFontVariantNumeric::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  if (range.peek().id() == CSSValueNormal)
    return CSSPropertyParserHelpers::consumeIdent(range);

  FontVariantNumericParser numericParser;
  do {
    if (numericParser.consumeNumeric(range) !=
        FontVariantNumericParser::ParseResult::ConsumedValue)
      return nullptr;
  } while (!range.atEnd());

  return numericParser.finalizeValue();
}

}  // namespace blink
