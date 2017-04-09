// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyMarginUtils.h"

namespace blink {

CSSValue* CSSPropertyMarginUtils::ConsumeMarginOrOffset(
    CSSParserTokenRange& range,
    CSSParserMode css_parser_mode,
    CSSPropertyParserHelpers::UnitlessQuirk unitless) {
  if (range.Peek().Id() == CSSValueAuto)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, css_parser_mode, kValueRangeAll, unitless);
}

}  // namespace blink
