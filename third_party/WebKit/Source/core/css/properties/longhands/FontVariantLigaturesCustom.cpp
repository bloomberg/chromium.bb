// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/FontVariantLigatures.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/parser/FontVariantLigaturesParser.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* FontVariantLigatures::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueNormal || range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  FontVariantLigaturesParser ligatures_parser;
  do {
    if (ligatures_parser.ConsumeLigature(range) !=
        FontVariantLigaturesParser::ParseResult::kConsumedValue)
      return nullptr;
  } while (!range.AtEnd());

  return ligatures_parser.FinalizeValue();
}

}  // namespace CSSLonghand
}  // namespace blink
