// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIFontSizeAdjust.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIFontSizeAdjust::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  DCHECK(RuntimeEnabledFeatures::CSSFontSizeAdjustEnabled());
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
}

}  // namespace blink
