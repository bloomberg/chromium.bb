// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/word_spacing.h"

#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/zoom_adjusted_pixel_value.h"

namespace blink {
namespace css_longhand {

const CSSValue* WordSpacing::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ParseSpacing(range, context);
}

const CSSValue* WordSpacing::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return ZoomAdjustedPixelValue(style.WordSpacing(), style);
}

}  // namespace css_longhand
}  // namespace blink
