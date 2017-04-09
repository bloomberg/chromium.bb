// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyColumnUtils.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

CSSValue* CSSPropertyColumnUtils::ConsumeColumnCount(
    CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueAuto)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumePositiveInteger(range);
}

CSSValue* CSSPropertyColumnUtils::ConsumeColumnWidth(
    CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueAuto)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  // Always parse lengths in strict mode here, since it would be ambiguous
  // otherwise when used in the 'columns' shorthand property.
  CSSPrimitiveValue* column_width = CSSPropertyParserHelpers::ConsumeLength(
      range, kHTMLStandardMode, kValueRangeNonNegative);
  if (!column_width ||
      (!column_width->IsCalculated() && column_width->GetDoubleValue() == 0))
    return nullptr;
  return column_width;
}

bool CSSPropertyColumnUtils::ConsumeColumnWidthOrCount(
    CSSParserTokenRange& range,
    CSSValue*& column_width,
    CSSValue*& column_count) {
  if (range.Peek().Id() == CSSValueAuto) {
    CSSPropertyParserHelpers::ConsumeIdent(range);
    return true;
  }
  if (!column_width) {
    column_width = ConsumeColumnWidth(range);
    if (column_width)
      return true;
  }
  if (!column_count)
    column_count = ConsumeColumnCount(range);
  return column_count;
}

}  // namespace blink
