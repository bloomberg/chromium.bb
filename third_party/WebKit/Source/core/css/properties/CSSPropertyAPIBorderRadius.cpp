// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIBorderRadius.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIBorderRadius::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSValue* parsedValue1 = CSSPropertyParserHelpers::consumeLengthOrPercent(
      range, context->mode(), ValueRangeNonNegative);
  if (!parsedValue1)
    return nullptr;
  CSSValue* parsedValue2 = CSSPropertyParserHelpers::consumeLengthOrPercent(
      range, context->mode(), ValueRangeNonNegative);
  if (!parsedValue2)
    parsedValue2 = parsedValue1;
  return CSSValuePair::create(parsedValue1, parsedValue2,
                              CSSValuePair::DropIdenticalValues);
}

}  // namespace blink
