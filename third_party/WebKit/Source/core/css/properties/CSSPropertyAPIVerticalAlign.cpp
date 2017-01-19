// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIVerticalAlign.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIVerticalAlign::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSValue* parsedValue = CSSPropertyParserHelpers::consumeIdentRange(
      range, CSSValueBaseline, CSSValueWebkitBaselineMiddle);
  if (!parsedValue) {
    parsedValue = CSSPropertyParserHelpers::consumeLengthOrPercent(
        range, context->mode(), ValueRangeAll,
        CSSPropertyParserHelpers::UnitlessQuirk::Allow);
  }
  return parsedValue;
}

}  // namespace blink
