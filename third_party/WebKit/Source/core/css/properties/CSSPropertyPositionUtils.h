// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CSSPropertyPositionUtils_h
#define CSSPropertyPositionUtils_h

#include "core/CSSValueKeywords.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserTokenRange;
class CSSValue;

class CSSPropertyPositionUtils {
  STATIC_ONLY(CSSPropertyPositionUtils);

 public:
  template <CSSValueID start, CSSValueID end>
  static CSSValue* ConsumePositionLonghand(CSSParserTokenRange& range,
                                           CSSParserMode css_parser_mode) {
    if (range.Peek().GetType() == kIdentToken) {
      CSSValueID id = range.Peek().Id();
      int percent;
      if (id == start)
        percent = 0;
      else if (id == CSSValueCenter)
        percent = 50;
      else if (id == end)
        percent = 100;
      else
        return nullptr;
      range.ConsumeIncludingWhitespace();
      return CSSPrimitiveValue::Create(
          percent, CSSPrimitiveValue::UnitType::kPercentage);
    }
    return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
        range, css_parser_mode, kValueRangeAll);
  }
};

}  // namespace blink

#endif  // CSSPropertyPositionUtils_h
