// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyFontUtils.h"

#include "core/css/CSSFontFamilyValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

CSSValue* CSSPropertyFontUtils::ConsumeFontSize(
    CSSParserTokenRange& range,
    CSSParserMode css_parser_mode,
    CSSPropertyParserHelpers::UnitlessQuirk unitless) {
  if (range.Peek().Id() >= CSSValueXxSmall &&
      range.Peek().Id() <= CSSValueLarger)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, css_parser_mode, kValueRangeNonNegative, unitless);
}

CSSValue* CSSPropertyFontUtils::ConsumeLineHeight(
    CSSParserTokenRange& range,
    CSSParserMode css_parser_mode) {
  if (range.Peek().Id() == CSSValueNormal)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSPrimitiveValue* line_height =
      CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
  if (line_height)
    return line_height;
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, css_parser_mode, kValueRangeNonNegative);
}

CSSValueList* CSSPropertyFontUtils::ConsumeFontFamily(
    CSSParserTokenRange& range) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  do {
    CSSValue* parsed_value = ConsumeGenericFamily(range);
    if (parsed_value) {
      list->Append(*parsed_value);
    } else {
      parsed_value = ConsumeFamilyName(range);
      if (parsed_value) {
        list->Append(*parsed_value);
      } else {
        return nullptr;
      }
    }
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));
  return list;
}

CSSValue* CSSPropertyFontUtils::ConsumeGenericFamily(
    CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::ConsumeIdentRange(range, CSSValueSerif,
                                                     CSSValueWebkitBody);
}

CSSValue* CSSPropertyFontUtils::ConsumeFamilyName(CSSParserTokenRange& range) {
  if (range.Peek().GetType() == kStringToken) {
    return CSSFontFamilyValue::Create(
        range.ConsumeIncludingWhitespace().Value().ToString());
  }
  if (range.Peek().GetType() != kIdentToken)
    return nullptr;
  String family_name = ConcatenateFamilyName(range);
  if (family_name.IsNull())
    return nullptr;
  return CSSFontFamilyValue::Create(family_name);
}

String CSSPropertyFontUtils::ConcatenateFamilyName(CSSParserTokenRange& range) {
  StringBuilder builder;
  bool added_space = false;
  const CSSParserToken& first_token = range.Peek();
  while (range.Peek().GetType() == kIdentToken) {
    if (!builder.IsEmpty()) {
      builder.Append(' ');
      added_space = true;
    }
    builder.Append(range.ConsumeIncludingWhitespace().Value());
  }
  if (!added_space &&
      CSSPropertyParserHelpers::IsCSSWideKeyword(first_token.Value()))
    return String();
  return builder.ToString();
}

CSSIdentifierValue* CSSPropertyFontUtils::ConsumeFontWeight(
    CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();
  if (token.Id() >= CSSValueNormal && token.Id() <= CSSValueLighter)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  if (token.GetType() != kNumberToken ||
      token.GetNumericValueType() != kIntegerValueType)
    return nullptr;
  int weight = static_cast<int>(token.NumericValue());
  if ((weight % 100) || weight < 100 || weight > 900)
    return nullptr;
  range.ConsumeIncludingWhitespace();
  return CSSIdentifierValue::Create(
      static_cast<CSSValueID>(CSSValue100 + weight / 100 - 1));
}

// TODO(bugsnash): move this to the FontFeatureSettings API when it is no longer
// being used by methods outside of the API
CSSValue* CSSPropertyFontUtils::ConsumeFontFeatureSettings(
    CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueNormal)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  CSSValueList* settings = CSSValueList::CreateCommaSeparated();
  do {
    CSSFontFeatureValue* font_feature_value =
        CSSPropertyFontUtils::ConsumeFontFeatureTag(range);
    if (!font_feature_value)
      return nullptr;
    settings->Append(*font_feature_value);
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));
  return settings;
}

CSSFontFeatureValue* CSSPropertyFontUtils::ConsumeFontFeatureTag(
    CSSParserTokenRange& range) {
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

  int tag_value = 1;
  // Feature tag values could follow: <integer> | on | off
  if (range.Peek().GetType() == kNumberToken &&
      range.Peek().GetNumericValueType() == kIntegerValueType &&
      range.Peek().NumericValue() >= 0) {
    tag_value = clampTo<int>(range.ConsumeIncludingWhitespace().NumericValue());
    if (tag_value < 0)
      return nullptr;
  } else if (range.Peek().Id() == CSSValueOn ||
             range.Peek().Id() == CSSValueOff) {
    tag_value = range.ConsumeIncludingWhitespace().Id() == CSSValueOn;
  }
  return CSSFontFeatureValue::Create(tag, tag_value);
}

CSSIdentifierValue* CSSPropertyFontUtils::ConsumeFontVariantCSS21(
    CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::ConsumeIdent<CSSValueNormal,
                                                CSSValueSmallCaps>(range);
}

}  // namespace blink
