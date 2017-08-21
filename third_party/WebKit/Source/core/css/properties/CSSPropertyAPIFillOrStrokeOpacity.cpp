// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIFillOrStrokeOpacity.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

class CSSParserLocalContext;

const CSSValue* CSSPropertyAPIFillOrStrokeOpacity::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeAll);
}

}  // namespace blink
