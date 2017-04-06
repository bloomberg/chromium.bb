// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIStrokeDasharray.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIStrokeDasharray::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSValueID id = range.peek().id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::consumeIdent(range);

  CSSValueList* dashes = CSSValueList::createCommaSeparated();
  do {
    CSSPrimitiveValue* dash = CSSPropertyParserHelpers::consumeLengthOrPercent(
        range, SVGAttributeMode, ValueRangeNonNegative);
    if (!dash ||
        (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range) &&
         range.atEnd()))
      return nullptr;
    dashes->append(*dash);
  } while (!range.atEnd());
  return dashes;
}

}  // namespace blink
