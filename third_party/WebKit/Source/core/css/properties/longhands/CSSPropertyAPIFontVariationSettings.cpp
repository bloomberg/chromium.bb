// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CSSPropertyAPIFontVariationSettings.h"

#include "core/css/CSSFontVariationValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

namespace {

CSSFontVariationValue* ConsumeFontVariationTag(CSSParserTokenRange& range) {
  // Feature tag name consists of 4-letter characters.
  static const unsigned kTagNameLength = 4;

  const CSSParserToken& token = range.ConsumeIncludingWhitespace();
  // Feature tag name comes first
  if (token.GetType() != kStringToken)
    return nullptr;
  if (token.Value().length() != kTagNameLength)
    return nullptr;
  AtomicString tag = token.Value().ToAtomicString();
  for (unsigned i = 0; i < kTagNameLength; ++i) {
    // Limits the range of characters to 0x20-0x7E, following the tag name rules
    // defined in the OpenType specification.
    UChar character = tag[i];
    if (character < 0x20 || character > 0x7E)
      return nullptr;
  }

  double tag_value = 0;
  if (!CSSPropertyParserHelpers::ConsumeNumberRaw(range, tag_value))
    return nullptr;
  return CSSFontVariationValue::Create(tag, clampTo<float>(tag_value));
}

}  // namespace

const CSSValue* CSSPropertyAPIFontVariationSettings::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSVariableFontsEnabled());
  if (range.Peek().Id() == CSSValueNormal)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  CSSValueList* variation_settings = CSSValueList::CreateCommaSeparated();
  do {
    CSSFontVariationValue* font_variation_value =
        ConsumeFontVariationTag(range);
    if (!font_variation_value)
      return nullptr;
    variation_settings->Append(*font_variation_value);
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));
  return variation_settings;
}

}  // namespace blink
