// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIFlexGrowOrShrink.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/Length.h"

namespace blink {

class CSSParserContext;
class CSSParserLocalContext;

const CSSValue* CSSPropertyAPIFlexGrowOrShrink::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) {
  return CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
}

}  // namespace blink
