// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIFontSizeAdjust.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

const CSSValue* CSSPropertyAPIFontSizeAdjust::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  DCHECK(RuntimeEnabledFeatures::cssFontSizeAdjustEnabled());
  if (range.peek().id() == CSSValueNone)
    return CSSPropertyParserHelpers::consumeIdent(range);
  return CSSPropertyParserHelpers::consumeNumber(range, ValueRangeNonNegative);
}

}  // namespace blink
