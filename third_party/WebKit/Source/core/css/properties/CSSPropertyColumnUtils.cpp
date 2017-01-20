// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyColumnUtils.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

CSSValue* CSSPropertyColumnUtils::consumeColumnCount(
    CSSParserTokenRange& range) {
  if (range.peek().id() == CSSValueAuto)
    return CSSPropertyParserHelpers::consumeIdent(range);
  return CSSPropertyParserHelpers::consumePositiveInteger(range);
}

CSSValue* CSSPropertyColumnUtils::consumeColumnWidth(
    CSSParserTokenRange& range) {
  if (range.peek().id() == CSSValueAuto)
    return CSSPropertyParserHelpers::consumeIdent(range);
  // Always parse lengths in strict mode here, since it would be ambiguous
  // otherwise when used in the 'columns' shorthand property.
  CSSPrimitiveValue* columnWidth = CSSPropertyParserHelpers::consumeLength(
      range, HTMLStandardMode, ValueRangeNonNegative);
  if (!columnWidth ||
      (!columnWidth->isCalculated() && columnWidth->getDoubleValue() == 0))
    return nullptr;
  return columnWidth;
}

bool CSSPropertyColumnUtils::consumeColumnWidthOrCount(
    CSSParserTokenRange& range,
    CSSValue*& columnWidth,
    CSSValue*& columnCount) {
  if (range.peek().id() == CSSValueAuto) {
    CSSPropertyParserHelpers::consumeIdent(range);
    return true;
  }
  if (!columnWidth) {
    columnWidth = consumeColumnWidth(range);
    if (columnWidth)
      return true;
  }
  if (!columnCount)
    columnCount = consumeColumnCount(range);
  return columnCount;
}

}  // namespace blink
