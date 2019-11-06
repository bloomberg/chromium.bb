// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_FONT_VARIANT_NUMERIC_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_FONT_VARIANT_NUMERIC_PARSER_H_

#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"

namespace blink {

class FontVariantNumericParser {
  STACK_ALLOCATED();

 public:
  FontVariantNumericParser()
      : saw_numeric_figure_value_(false),
        saw_numeric_spacing_value_(false),
        saw_numeric_fraction_value_(false),
        saw_ordinal_value_(false),
        saw_slashed_zero_value_(false),
        result_(CSSValueList::CreateSpaceSeparated()) {}

  enum class ParseResult { kConsumedValue, kDisallowedValue, kUnknownValue };

  ParseResult ConsumeNumeric(CSSParserTokenRange& range) {
    CSSValueID value_id = range.Peek().Id();
    switch (value_id) {
      case CSSValueID::kLiningNums:
      case CSSValueID::kOldstyleNums:
        if (saw_numeric_figure_value_)
          return ParseResult::kDisallowedValue;
        saw_numeric_figure_value_ = true;
        break;
      case CSSValueID::kProportionalNums:
      case CSSValueID::kTabularNums:
        if (saw_numeric_spacing_value_)
          return ParseResult::kDisallowedValue;
        saw_numeric_spacing_value_ = true;
        break;
      case CSSValueID::kDiagonalFractions:
      case CSSValueID::kStackedFractions:
        if (saw_numeric_fraction_value_)
          return ParseResult::kDisallowedValue;
        saw_numeric_fraction_value_ = true;
        break;
      case CSSValueID::kOrdinal:
        if (saw_ordinal_value_)
          return ParseResult::kDisallowedValue;
        saw_ordinal_value_ = true;
        break;
      case CSSValueID::kSlashedZero:
        if (saw_slashed_zero_value_)
          return ParseResult::kDisallowedValue;
        saw_slashed_zero_value_ = true;
        break;
      default:
        return ParseResult::kUnknownValue;
    }
    result_->Append(*css_property_parser_helpers::ConsumeIdent(range));
    return ParseResult::kConsumedValue;
  }

  CSSValue* FinalizeValue() {
    if (!result_->length())
      return CSSIdentifierValue::Create(CSSValueID::kNormal);
    return result_.Release();
  }

 private:
  bool saw_numeric_figure_value_;
  bool saw_numeric_spacing_value_;
  bool saw_numeric_fraction_value_;
  bool saw_ordinal_value_;
  bool saw_slashed_zero_value_;
  Member<CSSValueList> result_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_FONT_VARIANT_NUMERIC_PARSER_H_
