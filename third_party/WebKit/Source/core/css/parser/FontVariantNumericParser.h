// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontVariantNumericParser_h
#define FontVariantNumericParser_h

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

class FontVariantNumericParser {
  STACK_ALLOCATED();

 public:
  FontVariantNumericParser()
      : m_sawNumericFigureValue(false),
        m_sawNumericSpacingValue(false),
        m_sawNumericFractionValue(false),
        m_sawOrdinalValue(false),
        m_sawSlashedZeroValue(false),
        m_result(CSSValueList::createSpaceSeparated()) {}

  enum class ParseResult { ConsumedValue, DisallowedValue, UnknownValue };

  ParseResult consumeNumeric(CSSParserTokenRange& range) {
    CSSValueID valueID = range.peek().id();
    switch (valueID) {
      case CSSValueLiningNums:
      case CSSValueOldstyleNums:
        if (m_sawNumericFigureValue)
          return ParseResult::DisallowedValue;
        m_sawNumericFigureValue = true;
        break;
      case CSSValueProportionalNums:
      case CSSValueTabularNums:
        if (m_sawNumericSpacingValue)
          return ParseResult::DisallowedValue;
        m_sawNumericSpacingValue = true;
        break;
      case CSSValueDiagonalFractions:
      case CSSValueStackedFractions:
        if (m_sawNumericFractionValue)
          return ParseResult::DisallowedValue;
        m_sawNumericFractionValue = true;
        break;
      case CSSValueOrdinal:
        if (m_sawOrdinalValue)
          return ParseResult::DisallowedValue;
        m_sawOrdinalValue = true;
        break;
      case CSSValueSlashedZero:
        if (m_sawSlashedZeroValue)
          return ParseResult::DisallowedValue;
        m_sawSlashedZeroValue = true;
        break;
      default:
        return ParseResult::UnknownValue;
    }
    m_result->append(*CSSPropertyParserHelpers::consumeIdent(range));
    return ParseResult::ConsumedValue;
  }

  CSSValue* finalizeValue() {
    if (!m_result->length())
      return CSSIdentifierValue::create(CSSValueNormal);
    return m_result.release();
  }

 private:
  bool m_sawNumericFigureValue;
  bool m_sawNumericSpacingValue;
  bool m_sawNumericFractionValue;
  bool m_sawOrdinalValue;
  bool m_sawSlashedZeroValue;
  Member<CSSValueList> m_result;
};

}  // namespace blink

#endif  // FontVariantNumericParser_h
