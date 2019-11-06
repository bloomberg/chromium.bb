// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_SIZES_CALC_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_SIZES_CALC_PARSER_H_

// TODO(crbug.com/825895): Rename the file and classes from "calc" to "math"

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_math_operator.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

struct SizesCalcValue {
  DISALLOW_NEW();
  double value = 0;
  bool is_length = false;
  CSSMathOperator operation = CSSMathOperator::kInvalid;

  SizesCalcValue() = default;

  SizesCalcValue(double numeric_value, bool length)
      : value(numeric_value), is_length(length) {}

  explicit SizesCalcValue(CSSMathOperator op) : operation(op) {}
};

class CORE_EXPORT SizesCalcParser {
  STACK_ALLOCATED();

 public:
  SizesCalcParser(CSSParserTokenRange, MediaValues*);

  float Result() const;
  bool IsValid() const { return is_valid_; }

 private:
  bool CalcToReversePolishNotation(CSSParserTokenRange);
  bool Calculate();
  void AppendNumber(const CSSParserToken&);
  bool AppendLength(const CSSParserToken&);
  bool HandleComma(Vector<CSSParserToken>& stack, const CSSParserToken&);
  bool HandleRightParenthesis(Vector<CSSParserToken>& stack);
  bool HandleOperator(Vector<CSSParserToken>& stack, const CSSParserToken&);
  void AppendOperator(const CSSParserToken&);

  Vector<SizesCalcValue> value_list_;
  Member<MediaValues> media_values_;
  bool is_valid_;
  float result_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_SIZES_CALC_PARSER_H_
