// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIColumnGap.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIColumnGap::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  if (range.Peek().Id() == CSSValueNormal)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeLength(range, context.Mode(),
                                                 kValueRangeNonNegative);
}

}  // namespace blink
