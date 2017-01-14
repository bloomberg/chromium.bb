// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIFontVariationSettings.h"

#include "core/css/CSSFontVariationValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

namespace {

CSSFontVariationValue* consumeFontVariationTag(CSSParserTokenRange& range) {
  // Feature tag name consists of 4-letter characters.
  static const unsigned tagNameLength = 4;

  const CSSParserToken& token = range.consumeIncludingWhitespace();
  // Feature tag name comes first
  if (token.type() != StringToken)
    return nullptr;
  if (token.value().length() != tagNameLength)
    return nullptr;
  AtomicString tag = token.value().toAtomicString();
  for (unsigned i = 0; i < tagNameLength; ++i) {
    // Limits the range of characters to 0x20-0x7E, following the tag name rules
    // defined in the OpenType specification.
    UChar character = tag[i];
    if (character < 0x20 || character > 0x7E)
      return nullptr;
  }

  double tagValue = 0;
  if (!CSSPropertyParserHelpers::consumeNumberRaw(range, tagValue))
    return nullptr;
  return CSSFontVariationValue::create(tag, clampTo<float>(tagValue));
}

}  // namespace

const CSSValue* CSSPropertyAPIFontVariationSettings::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  DCHECK(RuntimeEnabledFeatures::cssVariableFontsEnabled());
  if (range.peek().id() == CSSValueNormal)
    return CSSPropertyParserHelpers::consumeIdent(range);
  CSSValueList* variationSettings = CSSValueList::createCommaSeparated();
  do {
    CSSFontVariationValue* fontVariationValue = consumeFontVariationTag(range);
    if (!fontVariationValue)
      return nullptr;
    variationSettings->append(*fontVariationValue);
  } while (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range));
  return variationSettings;
}

}  // namespace blink
