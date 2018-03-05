// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/MaxWidth.h"

#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* MaxWidth::ParseSingleValue(CSSParserTokenRange& range,
                                           const CSSParserContext& context,
                                           const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumeMaxWidthOrHeight(
      range, context, CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

const CSSValue* MaxWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  const Length& max_width = style.MaxWidth();
  if (max_width.IsMaxSizeNone())
    return CSSIdentifierValue::Create(CSSValueNone);
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(max_width, style);
}

}  // namespace CSSLonghand
}  // namespace blink
