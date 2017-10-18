// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CSSPropertyAPIStrokeDasharray.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIStrokeDasharray::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValueList* dashes = CSSValueList::CreateCommaSeparated();
  do {
    CSSPrimitiveValue* dash = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
        range, kSVGAttributeMode, kValueRangeNonNegative);
    if (!dash ||
        (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range) &&
         range.AtEnd()))
      return nullptr;
    dashes->Append(*dash);
  } while (!range.AtEnd());
  return dashes;
}

}  // namespace blink
