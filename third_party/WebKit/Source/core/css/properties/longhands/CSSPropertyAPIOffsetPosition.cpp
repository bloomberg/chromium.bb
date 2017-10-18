// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CSSPropertyAPIOffsetPosition.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/frame/UseCounter.h"

namespace blink {

using namespace CSSPropertyParserHelpers;

const CSSValue* CSSPropertyAPIOffsetPosition::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueAuto)
    return ConsumeIdent(range);
  CSSValue* value = ConsumePosition(range, context, UnitlessQuirk::kForbid,
                                    Optional<WebFeature>());

  // Count when we receive a valid position other than 'auto'.
  if (value && value->IsValuePair())
    context.Count(WebFeature::kCSSOffsetInEffect);
  return value;
}

}  // namespace blink
