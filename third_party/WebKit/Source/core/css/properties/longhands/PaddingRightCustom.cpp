// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/PaddingRight.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* PaddingRight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative,
      CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

bool PaddingRight::IsLayoutDependent(const ComputedStyle* style,
                                     LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingRight().IsFixed());
}

const CSSValue* PaddingRight::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  const Length& padding_right = style.PaddingRight();
  if (padding_right.IsFixed() || !layout_object || !layout_object->IsBox()) {
    return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(padding_right,
                                                               style);
  }
  return ZoomAdjustedPixelValue(
      ToLayoutBox(layout_object)->ComputedCSSPaddingRight(), style);
}

}  // namespace CSSLonghand
}  // namespace blink
