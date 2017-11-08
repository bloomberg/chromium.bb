// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/BorderRadiusCorner.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* BorderRadiusCorner::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValue* parsed_value1 = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative);
  if (!parsed_value1)
    return nullptr;
  CSSValue* parsed_value2 = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative);
  if (!parsed_value2)
    parsed_value2 = parsed_value1;
  return CSSValuePair::Create(parsed_value1, parsed_value2,
                              CSSValuePair::kDropIdenticalValues);
}

}  // namespace CSSLonghand
}  // namespace blink
