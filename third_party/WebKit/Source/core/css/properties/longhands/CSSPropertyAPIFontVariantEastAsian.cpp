// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CSSPropertyAPIFontVariantEastAsian.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/parser/FontVariantEastAsianParser.h"

namespace blink {

const CSSValue* CSSPropertyAPIFontVariantEastAsian::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueNormal)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  FontVariantEastAsianParser east_asian_parser;
  do {
    if (east_asian_parser.ConsumeEastAsian(range) !=
        FontVariantEastAsianParser::ParseResult::kConsumedValue)
      return nullptr;
  } while (!range.AtEnd());

  return east_asian_parser.FinalizeValue();
}

}  // namespace blink
