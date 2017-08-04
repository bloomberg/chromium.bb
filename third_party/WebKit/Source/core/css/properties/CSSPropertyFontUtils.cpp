// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyFontUtils.h"

#include "core/css/CSSFontFamilyValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSFontStyleRangeValue.h"
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
      (CSSPropertyParserHelpers::IsCSSWideKeyword(first_token.Value()) ||
       EqualIgnoringASCIICase(first_token.Value(), "default"))) {
    return String();
  }
  return builder.ToString();
}

static CSSValueList* CombineToRangeListOrNull(
    const CSSPrimitiveValue* range_start,
    const CSSPrimitiveValue* range_end) {
  DCHECK(range_start);
  DCHECK(range_end);
  if (range_end->GetFloatValue() < range_start->GetFloatValue())
    return nullptr;
  CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
  value_list->Append(*range_start);
  value_list->Append(*range_end);
  return value_list;
}

static bool IsAngleWithinLimits(CSSPrimitiveValue* angle) {
  constexpr float kMaxAngle = 90.0f;
  return angle->GetFloatValue() >= -kMaxAngle &&
         angle->GetFloatValue() <= kMaxAngle;
}

CSSValue* CSSPropertyFontUtils::ConsumeFontStyle(
    CSSParserTokenRange& range,
    const CSSParserMode& parser_mode) {
  if (range.Peek().Id() == CSSValueNormal ||
      range.Peek().Id() == CSSValueItalic)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  if (range.Peek().Id() != CSSValueOblique)
    return nullptr;

  CSSIdentifierValue* oblique_identifier =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueOblique>(range);

  CSSPrimitiveValue* start_angle =
      CSSPropertyParserHelpers::ConsumeAngle(range, nullptr, WTF::nullopt);
  if (!start_angle)
    return oblique_identifier;
  if (!IsAngleWithinLimits(start_angle))
    return nullptr;

  if (parser_mode != kCSSFontFaceRuleMode || range.AtEnd()) {
    CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
    value_list->Append(*start_angle);
    return CSSFontStyleRangeValue::Create(*oblique_identifier, *value_list);
  }

  CSSPrimitiveValue* end_angle =
      CSSPropertyParserHelpers::ConsumeAngle(range, nullptr, WTF::nullopt);
  if (!end_angle || !IsAngleWithinLimits(end_angle))
    return nullptr;

  CSSValueList* range_list = CombineToRangeListOrNull(start_angle, end_angle);
  if (!range_list)
    return nullptr;
  return CSSFontStyleRangeValue::Create(*oblique_identifier, *range_list);
}

CSSIdentifierValue* CSSPropertyFontUtils::ConsumeFontStretchKeywordOnly(
    CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();
  if (token.Id() == CSSValueNormal || (token.Id() >= CSSValueUltraCondensed &&
                                       token.Id() <= CSSValueUltraExpanded))
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return nullptr;
}

CSSValue* CSSPropertyFontUtils::ConsumeFontStretch(
    CSSParserTokenRange& range,
    const CSSParserMode& parser_mode) {
  CSSIdentifierValue* parsed_keyword = ConsumeFontStretchKeywordOnly(range);
  if (parsed_keyword)
    return parsed_keyword;

  CSSPrimitiveValue* start_percent =
      CSSPropertyParserHelpers::ConsumePercent(range, kValueRangeNonNegative);
  if (!start_percent || start_percent->GetFloatValue() <= 0)
    return nullptr;

  // In a non-font-face context, more than one percentage is not allowed.
  if (parser_mode != kCSSFontFaceRuleMode || range.AtEnd())
    return start_percent;

  CSSPrimitiveValue* end_percent =
      CSSPropertyParserHelpers::ConsumePercent(range, kValueRangeNonNegative);
  if (!end_percent || end_percent->GetFloatValue() <= 0)
    return nullptr;

  return CombineToRangeListOrNull(start_percent, end_percent);
}

CSSValue* CSSPropertyFontUtils::ConsumeFontWeight(
    CSSParserTokenRange& range,
    const CSSParserMode& parser_mode) {
  const CSSParserToken& token = range.Peek();
  if (token.Id() >= CSSValueNormal && token.Id() <= CSSValueLighter)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  // Avoid consuming the first zero of font: 0/0; e.g. in the Acid3 test.  In
  // font:0/0; the first zero is the font size, the second is the line height.
  // In font: 100 0/0; we should parse the first 100 as font-weight, the 0
  // before the slash as font size. We need to peek and check the token in order
  // to avoid parsing a 0 font size as a font-weight. If we call ConsumeNumber
  // straight away without Peek, then the parsing cursor advances too far and we
  // parsed font-size as font-weight incorrectly.
  if (token.GetType() == kNumberToken &&
      (token.NumericValue() < 1 || token.NumericValue() > 1000))
    return nullptr;

  CSSPrimitiveValue* start_weight =
      CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
  if (!start_weight || start_weight->GetFloatValue() < 1 ||
      start_weight->GetFloatValue() > 1000)
    return nullptr;

  // In a non-font-face context, more than one number is not allowed. Return
  // what we have. If there is trailing garbage, the AtEnd() check in
  // CSSPropertyParser::ParseValueStart will catch that.
  if (parser_mode != kCSSFontFaceRuleMode || range.AtEnd())
    return start_weight;

  CSSPrimitiveValue* end_weight =
      CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
  if (!end_weight || end_weight->GetFloatValue() < 1 ||
      end_weight->GetFloatValue() > 1000)
    return nullptr;

  return CombineToRangeListOrNull(start_weight, end_weight);
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
