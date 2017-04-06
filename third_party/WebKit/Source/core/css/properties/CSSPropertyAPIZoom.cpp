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
    const CSSParserContext* context) {
  const CSSParserToken& token = range.peek();
  CSSValue* zoom = nullptr;
  if (token.type() == IdentToken) {
    zoom = CSSPropertyParserHelpers::consumeIdent<CSSValueNormal, CSSValueReset,
                                                  CSSValueDocument>(range);
  } else {
    zoom =
        CSSPropertyParserHelpers::consumePercent(range, ValueRangeNonNegative);
    if (!zoom) {
      zoom =
          CSSPropertyParserHelpers::consumeNumber(range, ValueRangeNonNegative);
    }
  }
  if (zoom) {
    if (!(token.id() == CSSValueNormal ||
          (token.type() == NumberToken &&
           toCSSPrimitiveValue(zoom)->getDoubleValue() == 1) ||
          (token.type() == PercentageToken &&
           toCSSPrimitiveValue(zoom)->getDoubleValue() == 100)))
      context->count(UseCounter::CSSZoomNotEqualToOne);
    if (token.id() == CSSValueReset)
      context->countDeprecation(UseCounter::CSSZoomReset);
    if (token.id() == CSSValueDocument)
      context->countDeprecation(UseCounter::CSSZoomDocument);
  }
  return zoom;
}

}  // namespace blink
