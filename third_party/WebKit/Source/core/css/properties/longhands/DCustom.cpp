// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/D.h"

#include "core/css/properties/CSSParsingUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* D::ParseSingleValue(CSSParserTokenRange& range,
                                    const CSSParserContext&,
                                    const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumePathOrNone(range);
}

const CSSValue* D::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  if (const StylePath* style_path = svg_style.D())
    return style_path->ComputedCSSValue();
  return CSSIdentifierValue::Create(CSSValueNone);
}

}  // namespace CSSLonghand
}  // namespace blink
