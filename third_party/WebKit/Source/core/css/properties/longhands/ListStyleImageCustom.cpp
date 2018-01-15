// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/ListStyleImage.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/style/ComputedStyle.h"

namespace blink {

class CSSParserLocalContext;

namespace CSSLonghand {

const CSSValue* ListStyleImage::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeImageOrNone(range, &context);
}

const CSSValue* ListStyleImage::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  if (style.ListStyleImage())
    return style.ListStyleImage()->ComputedCSSValue();
  return CSSIdentifierValue::Create(CSSValueNone);
}

}  // namespace CSSLonghand
}  // namespace blink
