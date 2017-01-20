// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CSSPropertyPositionUtils_h
#define CSSPropertyPositionUtils_h

#include "core/CSSValueKeywords.h"
#include "core/css/parser/CSSParserMode.h"

#include "wtf/Allocator.h"

namespace blink {

class CSSParserTokenRange;
class CSSValue;

class CSSPropertyPositionUtils {
  STATIC_ONLY(CSSPropertyPositionUtils);

  template <CSSValueID start, CSSValueID end>
  static CSSValue* consumePositionLonghand(CSSParserTokenRange& range,
                                           CSSParserMode cssParserMode) {
    if (range.peek().type() == IdentToken) {
      CSSValueID id = range.peek().id();
      int percent;
      if (id == start)
        percent = 0;
      else if (id == CSSValueCenter)
        percent = 50;
      else if (id == end)
        percent = 100;
      else
        return nullptr;
      range.consumeIncludingWhitespace();
      return CSSPrimitiveValue::create(percent,
                                       CSSPrimitiveValue::UnitType::Percentage);
    }
    return CSSPropertyParserHelpers::consumeLengthOrPercent(
        range, cssParserMode, ValueRangeAll);
  }
};

}  // namespace blink

#endif  // CSSPropertyPositionUtils_h
