// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/StrokeDashoffset.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/ComputedStyleUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* StrokeDashoffset::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, kSVGAttributeMode, kValueRangeAll,
      CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
}

const CSSValue* StrokeDashoffset::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.StrokeDashOffset(), style);
}

}  // namespace CSSLonghand
}  // namespace blink
