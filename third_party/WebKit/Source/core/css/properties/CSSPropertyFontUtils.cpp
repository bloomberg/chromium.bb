// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyFontUtils.h"

#include "core/css/CSSFontFamilyValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

CSSValue* CSSPropertyFontUtils::consumeFontSize(
    CSSParserTokenRange& range,
    CSSParserMode cssParserMode,
    CSSPropertyParserHelpers::UnitlessQuirk unitless) {
  if (range.peek().id() >= CSSValueXxSmall &&
      range.peek().id() <= CSSValueLarger)
    return CSSPropertyParserHelpers::consumeIdent(range);
  return CSSPropertyParserHelpers::consumeLengthOrPercent(
      range, cssParserMode, ValueRangeNonNegative, unitless);
}

CSSValue* CSSPropertyFontUtils::consumeLineHeight(CSSParserTokenRange& range,
                                                  CSSParserMode cssParserMode) {
  if (range.peek().id() == CSSValueNormal)
    return CSSPropertyParserHelpers::consumeIdent(range);

  CSSPrimitiveValue* lineHeight =
      CSSPropertyParserHelpers::consumeNumber(range, ValueRangeNonNegative);
  if (lineHeight)
    return lineHeight;
  return CSSPropertyParserHelpers::consumeLengthOrPercent(
      range, cssParserMode, ValueRangeNonNegative);
}

CSSValueList* CSSPropertyFontUtils::consumeFontFamily(
    CSSParserTokenRange& range) {
  CSSValueList* list = CSSValueList::createCommaSeparated();
  do {
    CSSValue* parsedValue = consumeGenericFamily(range);
    if (parsedValue) {
      list->append(*parsedValue);
    } else {
      parsedValue = consumeFamilyName(range);
      if (parsedValue) {
        list->append(*parsedValue);
      } else {
        return nullptr;
      }
    }
  } while (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range));
  return list;
}

CSSValue* CSSPropertyFontUtils::consumeGenericFamily(
    CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::consumeIdentRange(range, CSSValueSerif,
                                                     CSSValueWebkitBody);
}

CSSValue* CSSPropertyFontUtils::consumeFamilyName(CSSParserTokenRange& range) {
  if (range.peek().type() == StringToken) {
    return CSSFontFamilyValue::create(
        range.consumeIncludingWhitespace().value().toString());
  }
  if (range.peek().type() != IdentToken)
    return nullptr;
  String familyName = concatenateFamilyName(range);
  if (familyName.isNull())
    return nullptr;
  return CSSFontFamilyValue::create(familyName);
}

String CSSPropertyFontUtils::concatenateFamilyName(CSSParserTokenRange& range) {
  StringBuilder builder;
  bool addedSpace = false;
  const CSSParserToken& firstToken = range.peek();
  while (range.peek().type() == IdentToken) {
    if (!builder.isEmpty()) {
      builder.append(' ');
      addedSpace = true;
    }
    builder.append(range.consumeIncludingWhitespace().value());
  }
  if (!addedSpace &&
      CSSPropertyParserHelpers::isCSSWideKeyword(firstToken.value()))
    return String();
  return builder.toString();
}

CSSIdentifierValue* CSSPropertyFontUtils::consumeFontWeight(
    CSSParserTokenRange& range) {
  const CSSParserToken& token = range.peek();
  if (token.id() >= CSSValueNormal && token.id() <= CSSValueLighter)
    return CSSPropertyParserHelpers::consumeIdent(range);
  if (token.type() != NumberToken ||
      token.numericValueType() != IntegerValueType)
    return nullptr;
  int weight = static_cast<int>(token.numericValue());
  if ((weight % 100) || weight < 100 || weight > 900)
    return nullptr;
  range.consumeIncludingWhitespace();
  return CSSIdentifierValue::create(
      static_cast<CSSValueID>(CSSValue100 + weight / 100 - 1));
}

// TODO(bugsnash): move this to the FontFeatureSettings API when it is no longer
// being used by methods outside of the API
CSSValue* CSSPropertyFontUtils::consumeFontFeatureSettings(
    CSSParserTokenRange& range) {
  if (range.peek().id() == CSSValueNormal)
    return CSSPropertyParserHelpers::consumeIdent(range);
  CSSValueList* settings = CSSValueList::createCommaSeparated();
  do {
    CSSFontFeatureValue* fontFeatureValue =
        CSSPropertyFontUtils::consumeFontFeatureTag(range);
    if (!fontFeatureValue)
      return nullptr;
    settings->append(*fontFeatureValue);
  } while (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range));
  return settings;
}

CSSFontFeatureValue* CSSPropertyFontUtils::consumeFontFeatureTag(
    CSSParserTokenRange& range) {
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

  int tagValue = 1;
  // Feature tag values could follow: <integer> | on | off
  if (range.peek().type() == NumberToken &&
      range.peek().numericValueType() == IntegerValueType &&
      range.peek().numericValue() >= 0) {
    tagValue = clampTo<int>(range.consumeIncludingWhitespace().numericValue());
    if (tagValue < 0)
      return nullptr;
  } else if (range.peek().id() == CSSValueOn ||
             range.peek().id() == CSSValueOff) {
    tagValue = range.consumeIncludingWhitespace().id() == CSSValueOn;
  }
  return CSSFontFeatureValue::create(tag, tagValue);
}

}  // namespace blink
