// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAnimationIterationCountUtils.h"

#include "core/CSSValueKeywords.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/Length.h"

namespace blink {

CSSValue*
CSSPropertyAnimationIterationCountUtils::ConsumeAnimationIterationCount(
    CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueInfinite)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeNonNegative);
}

}  // namespace blink
