// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/WebkitMaskBoxImageSource.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/style/ComputedStyle.h"

namespace blink {

class CSSParserLocalContext;

namespace CSSLonghand {

const CSSValue* WebkitMaskBoxImageSource::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeImageOrNone(range, &context);
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

}  // namespace CSSLonghand
}  // namespace blink
