// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIScrollPadding.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIScrollPadding::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  return ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative,
      CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

}  // namespace blink
