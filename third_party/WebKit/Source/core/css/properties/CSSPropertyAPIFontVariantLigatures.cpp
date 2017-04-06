// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIFontVariantLigatures.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/parser/FontVariantLigaturesParser.h"

namespace blink {

const CSSValue* CSSPropertyAPIFontVariantLigatures::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  if (range.peek().id() == CSSValueNormal || range.peek().id() == CSSValueNone)
    return CSSPropertyParserHelpers::consumeIdent(range);

  FontVariantLigaturesParser ligaturesParser;
  do {
    if (ligaturesParser.consumeLigature(range) !=
        FontVariantLigaturesParser::ParseResult::ConsumedValue)
      return nullptr;
  } while (!range.atEnd());

  return ligaturesParser.finalizeValue();
}

}  // namespace blink
