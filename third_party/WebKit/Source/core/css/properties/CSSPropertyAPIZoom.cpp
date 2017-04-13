// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIZoom.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/frame/UseCounter.h"

namespace blink {

const CSSValue* CSSPropertyAPIZoom::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPropertyID) {
  const CSSParserToken& token = range.Peek();
  CSSValue* zoom = nullptr;
  if (token.GetType() == kIdentToken) {
    zoom = CSSPropertyParserHelpers::ConsumeIdent<CSSValueNormal, CSSValueReset,
                                                  CSSValueDocument>(range);
  } else {
    zoom =
        CSSPropertyParserHelpers::ConsumePercent(range, kValueRangeNonNegative);
    if (!zoom) {
      zoom = CSSPropertyParserHelpers::ConsumeNumber(range,
                                                     kValueRangeNonNegative);
    }
  }
  if (zoom) {
    if (!(token.Id() == CSSValueNormal ||
          (token.GetType() == kNumberToken &&
           ToCSSPrimitiveValue(zoom)->GetDoubleValue() == 1) ||
          (token.GetType() == kPercentageToken &&
           ToCSSPrimitiveValue(zoom)->GetDoubleValue() == 100)))
      context.Count(UseCounter::kCSSZoomNotEqualToOne);
    if (token.Id() == CSSValueReset)
      context.CountDeprecation(UseCounter::kCSSZoomReset);
    if (token.Id() == CSSValueDocument)
      context.CountDeprecation(UseCounter::kCSSZoomDocument);
  }
  return zoom;
}

}  // namespace blink
