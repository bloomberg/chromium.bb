// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIFlexBasis.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIFlexBasis::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  // FIXME: Support intrinsic dimensions too.
  if (range.peek().id() == CSSValueAuto)
    return CSSPropertyParserHelpers::consumeIdent(range);
  return CSSPropertyParserHelpers::consumeLengthOrPercent(
      range, context->mode(), ValueRangeNonNegative);
}

}  // namespace blink
