// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/Zoom.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/frame/UseCounter.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* Zoom::ParseSingleValue(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  const CSSParserToken& token = range.Peek();
  CSSValue* zoom = nullptr;
  if (token.GetType() == kIdentToken) {
    zoom = CSSPropertyParserHelpers::ConsumeIdent<CSSValueNormal>(range);
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
      context.Count(WebFeature::kCSSZoomNotEqualToOne);
  }
  return zoom;
}

const CSSValue* Zoom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return CSSPrimitiveValue::Create(style.Zoom(),
                                   CSSPrimitiveValue::UnitType::kNumber);
}

}  // namespace CSSLonghand
}  // namespace blink
