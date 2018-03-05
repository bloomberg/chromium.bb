// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/Height.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* Height::ParseSingleValue(CSSParserTokenRange& range,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumeWidthOrHeight(
      range, context, CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

bool Height::IsLayoutDependent(const ComputedStyle* style,
                               LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* Height::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  if (ComputedStyleUtils::WidthOrHeightShouldReturnUsedValue(layout_object)) {
    return ZoomAdjustedPixelValue(
        ComputedStyleUtils::SizingBox(*layout_object).Height(), style);
  }
  return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(style.Height(),
                                                             style);
}

}  // namespace CSSLonghand
}  // namespace blink
