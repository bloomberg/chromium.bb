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
      : saw_common_ligatures_value_(false),
        saw_discretionary_ligatures_value_(false),
        saw_historical_ligatures_value_(false),
        saw_contextual_ligatures_value_(false),
        result_(CSSValueList::CreateSpaceSeparated()) {}

  enum class ParseResult { kConsumedValue, kDisallowedValue, kUnknownValue };

  ParseResult ConsumeLigature(CSSParserTokenRange& range) {
    CSSValueID value_id = range.Peek().Id();
    switch (value_id) {
      case CSSValueNoCommonLigatures:
      case CSSValueCommonLigatures:
        if (saw_common_ligatures_value_)
          return ParseResult::kDisallowedValue;
        saw_common_ligatures_value_ = true;
        break;
      case CSSValueNoDiscretionaryLigatures:
      case CSSValueDiscretionaryLigatures:
        if (saw_discretionary_ligatures_value_)
          return ParseResult::kDisallowedValue;
        saw_discretionary_ligatures_value_ = true;
        break;
      case CSSValueNoHistoricalLigatures:
      case CSSValueHistoricalLigatures:
        if (saw_historical_ligatures_value_)
          return ParseResult::kDisallowedValue;
        saw_historical_ligatures_value_ = true;
        break;
      case CSSValueNoContextual:
      case CSSValueContextual:
        if (saw_contextual_ligatures_value_)
          return ParseResult::kDisallowedValue;
        saw_contextual_ligatures_value_ = true;
        break;
      default:
        return ParseResult::kUnknownValue;
    }
    result_->Append(*CSSPropertyParserHelpers::ConsumeIdent(range));
    return ParseResult::kConsumedValue;
  }

  CSSValue* FinalizeValue() {
    if (!result_->length())
      return CSSIdentifierValue::Create(CSSValueNormal);
    return result_.Release();
  }

 private:
  bool saw_common_ligatures_value_;
  bool saw_discretionary_ligatures_value_;
  bool saw_historical_ligatures_value_;
  bool saw_contextual_ligatures_value_;
  Member<CSSValueList> result_;
};

}  // namespace blink

#endif  // FontVariantLigaturesParser_h
