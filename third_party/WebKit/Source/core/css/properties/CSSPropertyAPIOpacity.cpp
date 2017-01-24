// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIOpacity.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIOpacity::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  return CSSPropertyParserHelpers::consumeNumber(range, ValueRangeAll);
}

}  // namespace blink
