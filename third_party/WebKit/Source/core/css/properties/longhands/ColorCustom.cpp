// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/Color.h"

#include "core/css/CSSColorValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* Color::ParseSingleValue(CSSParserTokenRange& range,
                                        const CSSParserContext& context,
                                        const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeColor(
      range, context.Mode(), IsQuirksModeBehavior(context.Mode()));
}

const blink::Color Color::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  StyleColor result =
      visited_link ? style.VisitedLinkColor() : style.GetColor();
  ;
  return result.GetColor();
}

const CSSValue* Color::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return cssvalue::CSSColorValue::Create(
      allow_visited_style ? style.VisitedDependentColor(*this).Rgb()
                          : style.GetColor().Rgb());
}

}  // namespace CSSLonghand
}  // namespace blink
