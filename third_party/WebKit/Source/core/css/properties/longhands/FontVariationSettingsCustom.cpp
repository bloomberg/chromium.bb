// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/FontVariationSettings.h"

#include "core/css/CSSFontVariationValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/style/ComputedStyle.h"
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
namespace CSSLonghand {

const CSSValue* FontVariationSettings::ParseSingleValue(
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

const CSSValue* FontVariationSettings::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  DCHECK(RuntimeEnabledFeatures::CSSVariableFontsEnabled());
  const blink::FontVariationSettings* variation_settings =
      style.GetFontDescription().VariationSettings();
  if (!variation_settings || !variation_settings->size())
    return CSSIdentifierValue::Create(CSSValueNormal);
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (unsigned i = 0; i < variation_settings->size(); ++i) {
    const FontVariationAxis& variation_axis = variation_settings->at(i);
    CSSFontVariationValue* variation_value = CSSFontVariationValue::Create(
        variation_axis.Tag(), variation_axis.Value());
    list->Append(*variation_value);
  }
  return list;
}

}  // namespace CSSLonghand
}  // namespace blink
