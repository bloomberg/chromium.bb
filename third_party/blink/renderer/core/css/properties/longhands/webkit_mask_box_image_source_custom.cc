// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/webkit_mask_box_image_source.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

class CSSParserLocalContext;

namespace css_longhand {

const CSSValue* WebkitMaskBoxImageSource::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_property_parser_helpers::ConsumeImageOrNone(range, &context);
}

const CSSValue* WebkitMaskBoxImageSource::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  if (style.MaskBoxImageSource())
    return style.MaskBoxImageSource()->ComputedCSSValue();
  return CSSIdentifierValue::Create(CSSValueNone);
}

void WebkitMaskBoxImageSource::ApplyValue(StyleResolverState& state,
                                          const CSSValue& value) const {
  state.Style()->SetMaskBoxImageSource(
      state.GetStyleImage(CSSPropertyWebkitMaskBoxImageSource, value));
}

}  // namespace css_longhand
}  // namespace blink
