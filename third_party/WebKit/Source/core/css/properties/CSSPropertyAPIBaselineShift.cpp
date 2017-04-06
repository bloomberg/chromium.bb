// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIBaselineShift.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIBaselineShift::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSValueID id = range.peek().id();
  if (id == CSSValueBaseline || id == CSSValueSub || id == CSSValueSuper)
    return CSSPropertyParserHelpers::consumeIdent(range);
  return CSSPropertyParserHelpers::consumeLengthOrPercent(
      range, SVGAttributeMode, ValueRangeAll);
}

}  // namespace blink
