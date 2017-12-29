// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/WebkitTextStrokeColor.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {

class CSSParserLocalContext;

namespace CSSLonghand {

const CSSValue* WebkitTextStrokeColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeColor(range, context.Mode());
}

const blink::Color WebkitTextStrokeColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  StyleColor result = visited_link ? style.VisitedLinkTextStrokeColor()
                                   : style.TextStrokeColor();
  if (!result.IsCurrentColor())
    return result.GetColor();
  return visited_link ? style.VisitedLinkColor() : style.GetColor();
}

const CSSValue* WebkitTextStrokeColor::CSSValueFromComputedStyle(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::CurrentColorOrValidColor(style,
                                                      style.TextStrokeColor());
}

}  // namespace CSSLonghand
}  // namespace blink
