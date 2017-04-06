// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPITabSize.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPITabSize::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSPrimitiveValue* parsedValue =
      CSSPropertyParserHelpers::consumeInteger(range, 0);
  if (parsedValue)
    return parsedValue;
  return CSSPropertyParserHelpers::consumeLength(range, context->mode(),
                                                 ValueRangeNonNegative);
}

}  // namespace blink
