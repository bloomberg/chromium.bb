// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPISize.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

class CSSParserLocalContext;
namespace blink {

static CSSValue* ConsumePageSize(CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::ConsumeIdent<
      CSSValueA3, CSSValueA4, CSSValueA5, CSSValueB4, CSSValueB5,
      CSSValueLedger, CSSValueLegal, CSSValueLetter>(range);
}

const CSSValue* CSSPropertyAPISize::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  CSSValueList* result = CSSValueList::CreateSpaceSeparated();

  if (range.Peek().Id() == CSSValueAuto) {
    result->Append(*CSSPropertyParserHelpers::ConsumeIdent(range));
    return result;
  }

  if (CSSValue* width = CSSPropertyParserHelpers::ConsumeLength(
          range, context.Mode(), kValueRangeNonNegative)) {
    CSSValue* height = CSSPropertyParserHelpers::ConsumeLength(
        range, context.Mode(), kValueRangeNonNegative);
    result->Append(*width);
    if (height)
      result->Append(*height);
    return result;
  }

  CSSValue* page_size = ConsumePageSize(range);
  CSSValue* orientation =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValuePortrait,
                                             CSSValueLandscape>(range);
  if (!page_size)
    page_size = ConsumePageSize(range);

  if (!orientation && !page_size)
    return nullptr;
  if (page_size)
    result->Append(*page_size);
  if (orientation)
    result->Append(*orientation);
  return result;
}

}  // namespace blink
