// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/MarginRight.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* MarginRight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumeMarginOrOffset(
      range, context.Mode(), CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

bool MarginRight::IsLayoutDependent(const ComputedStyle* style,
                                    LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->MarginRight().IsFixed());
}

const CSSValue* MarginRight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  const Length& margin_right = style.MarginRight();
  if (margin_right.IsFixed() || !layout_object || !layout_object->IsBox()) {
    return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(margin_right,
                                                               style);
  }
  float value;
  if (margin_right.IsPercentOrCalc()) {
    // LayoutBox gives a marginRight() that is the distance between the
    // right-edge of the child box and the right-edge of the containing box,
    // when display == EDisplay::kBlock. Let's calculate the absolute value
    // of the specified margin-right % instead of relying on LayoutBox's
    // marginRight() value.
    value =
        MinimumValueForLength(
            margin_right,
            ToLayoutBox(layout_object)->ContainingBlockLogicalWidthForContent())
            .ToFloat();
  } else {
    value = ToLayoutBox(layout_object)->MarginRight().ToFloat();
  }
  return ZoomAdjustedPixelValue(value, style);
}

}  // namespace CSSLonghand
}  // namespace blink
