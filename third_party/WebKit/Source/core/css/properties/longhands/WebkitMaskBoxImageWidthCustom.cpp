// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/WebkitMaskBoxImageWidth.h"

#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* WebkitMaskBoxImageWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumeBorderImageWidth(range);
}

const CSSValue* WebkitMaskBoxImageWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImageQuad(
      style.MaskBoxImage().BorderSlices(), style);
}

}  // namespace CSSLonghand
}  // namespace blink
