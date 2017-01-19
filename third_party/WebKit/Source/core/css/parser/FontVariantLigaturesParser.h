// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FontVariantLigaturesParser_h
#define FontVariantLigaturesParser_h

#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

class FontVariantLigaturesParser {
  STACK_ALLOCATED();

 public:
  FontVariantLigaturesParser()
      : m_sawCommonLigaturesValue(false),
        m_sawDiscretionaryLigaturesValue(false),
        m_sawHistoricalLigaturesValue(false),
        m_sawContextualLigaturesValue(false),
        m_result(CSSValueList::createSpaceSeparated()) {}

  enum class ParseResult { ConsumedValue, DisallowedValue, UnknownValue };

  ParseResult consumeLigature(CSSParserTokenRange& range) {
    CSSValueID valueID = range.peek().id();
    switch (valueID) {
      case CSSValueNoCommonLigatures:
      case CSSValueCommonLigatures:
        if (m_sawCommonLigaturesValue)
          return ParseResult::DisallowedValue;
        m_sawCommonLigaturesValue = true;
        break;
      case CSSValueNoDiscretionaryLigatures:
      case CSSValueDiscretionaryLigatures:
        if (m_sawDiscretionaryLigaturesValue)
          return ParseResult::DisallowedValue;
        m_sawDiscretionaryLigaturesValue = true;
        break;
      case CSSValueNoHistoricalLigatures:
      case CSSValueHistoricalLigatures:
        if (m_sawHistoricalLigaturesValue)
          return ParseResult::DisallowedValue;
        m_sawHistoricalLigaturesValue = true;
        break;
      case CSSValueNoContextual:
      case CSSValueContextual:
        if (m_sawContextualLigaturesValue)
          return ParseResult::DisallowedValue;
        m_sawContextualLigaturesValue = true;
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
  bool m_sawCommonLigaturesValue;
  bool m_sawDiscretionaryLigaturesValue;
  bool m_sawHistoricalLigaturesValue;
  bool m_sawContextualLigaturesValue;
  Member<CSSValueList> m_result;
};

}  // namespace blink

#endif  // FontVariantLigaturesParser_h
