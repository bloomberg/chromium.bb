// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/MarginTop.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* MarginTop::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumeMarginOrOffset(
      range, context.Mode(), CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

bool MarginTop::IsLayoutDependent(const ComputedStyle* style,
                                  LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->MarginTop().IsFixed());
}

const CSSValue* MarginTop::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  const Length& margin_top = style.MarginTop();
  if (margin_top.IsFixed() || !layout_object || !layout_object->IsBox()) {
    return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(margin_top,
                                                               style);
  }
  return ZoomAdjustedPixelValue(ToLayoutBox(layout_object)->MarginTop(), style);
}

}  // namespace CSSLonghand
}  // namespace blink
