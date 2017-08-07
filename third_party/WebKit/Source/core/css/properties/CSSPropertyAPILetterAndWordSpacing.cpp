// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPILetterAndWordSpacing.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPILetterAndWordSpacing::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueNormal)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  // TODO(timloh): allow <percentage>s in word-spacing.
  return CSSPropertyParserHelpers::ConsumeLength(
      range, context.Mode(), kValueRangeAll,
      CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

}  // namespace blink
