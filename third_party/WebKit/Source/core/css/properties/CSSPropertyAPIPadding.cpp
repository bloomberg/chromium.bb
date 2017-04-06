// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIPadding.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIPadding::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  return consumeLengthOrPercent(range, context->mode(), ValueRangeNonNegative,
                                CSSPropertyParserHelpers::UnitlessQuirk::Allow);
}

}  // namespace blink
