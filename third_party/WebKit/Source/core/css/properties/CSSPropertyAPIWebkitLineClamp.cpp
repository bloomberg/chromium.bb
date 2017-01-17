// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWebkitLineClamp.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIWebkitLineClamp::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  if (range.peek().type() != PercentageToken &&
      range.peek().type() != NumberToken)
    return nullptr;
  CSSPrimitiveValue* clampValue =
      CSSPropertyParserHelpers::consumePercent(range, ValueRangeNonNegative);
  if (clampValue)
    return clampValue;
  // When specifying number of lines, don't allow 0 as a valid value.
  return CSSPropertyParserHelpers::consumePositiveInteger(range);
}

}  // namespace blink
