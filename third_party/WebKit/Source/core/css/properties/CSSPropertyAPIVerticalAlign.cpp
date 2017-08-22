// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIVerticalAlign.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIVerticalAlign::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValue* parsed_value = CSSPropertyParserHelpers::ConsumeIdentRange(
      range, CSSValueBaseline, CSSValueWebkitBaselineMiddle);
  if (!parsed_value) {
    parsed_value = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
        range, context.Mode(), kValueRangeAll,
        CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
  }
  return parsed_value;
}

}  // namespace blink
