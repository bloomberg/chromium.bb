// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/FontSize.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* FontSize::ParseSingleValue(CSSParserTokenRange& range,
                                           const CSSParserContext& context,
                                           const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumeFontSize(
      range, context.Mode(), CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

const CSSValue* FontSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFontSize(style);
}

}  // namespace CSSLonghand
}  // namespace blink
