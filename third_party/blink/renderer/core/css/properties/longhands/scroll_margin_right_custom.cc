// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/scroll_margin_right.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/zoom_adjusted_pixel_value.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* ScrollMarginRight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context.Mode(), kValueRangeAll,
                       CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
}

const CSSValue* ScrollMarginRight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.ScrollMarginRight(), style);
}

}  // namespace CSSLonghand
}  // namespace blink
