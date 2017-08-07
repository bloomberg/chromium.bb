// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIBaselineShift.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIBaselineShift::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueBaseline || id == CSSValueSub || id == CSSValueSuper)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, kSVGAttributeMode, kValueRangeAll);
}

}  // namespace blink
