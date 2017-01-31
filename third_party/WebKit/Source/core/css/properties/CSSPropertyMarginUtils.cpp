// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyMarginUtils.h"

namespace blink {

CSSValue* CSSPropertyMarginUtils::consumeMarginOrOffset(
    CSSParserTokenRange& range,
    CSSParserMode cssParserMode,
    CSSPropertyParserHelpers::UnitlessQuirk unitless) {
  if (range.peek().id() == CSSValueAuto)
    return CSSPropertyParserHelpers::consumeIdent(range);
  return CSSPropertyParserHelpers::consumeLengthOrPercent(
      range, cssParserMode, ValueRangeAll, unitless);
}

}  // namespace blink
