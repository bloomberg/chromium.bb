// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIFillOrStrokeOpacity.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIFillOrStrokeOpacity::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  return CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeAll);
}

}  // namespace blink
