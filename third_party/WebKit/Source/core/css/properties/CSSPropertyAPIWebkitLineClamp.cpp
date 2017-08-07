// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWebkitLineClamp.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIWebkitLineClamp::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().GetType() != kPercentageToken &&
      range.Peek().GetType() != kNumberToken)
    return nullptr;
  CSSPrimitiveValue* clamp_value =
      CSSPropertyParserHelpers::ConsumePercent(range, kValueRangeNonNegative);
  if (clamp_value)
    return clamp_value;
  // When specifying number of lines, don't allow 0 as a valid value.
  return CSSPropertyParserHelpers::ConsumePositiveInteger(range);
}

}  // namespace blink
