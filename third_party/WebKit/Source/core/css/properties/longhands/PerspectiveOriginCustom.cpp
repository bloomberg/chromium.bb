// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/PerspectiveOrigin.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/frame/WebFeature.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* PerspectiveOrigin::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumePosition(range, context,
                         CSSPropertyParserHelpers::UnitlessQuirk::kForbid,
                         WebFeature::kThreeValuedPositionPerspectiveOrigin);
}

bool PerspectiveOrigin::IsLayoutDependent(const ComputedStyle* style,
                                          LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* PerspectiveOrigin::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (layout_object) {
    LayoutRect box;
    if (layout_object->IsBox())
      box = ToLayoutBox(layout_object)->BorderBoxRect();

    list->Append(*ZoomAdjustedPixelValue(
        MinimumValueForLength(style.PerspectiveOriginX(), box.Width()), style));
    list->Append(*ZoomAdjustedPixelValue(
        MinimumValueForLength(style.PerspectiveOriginY(), box.Height()),
        style));
  } else {
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        style.PerspectiveOriginX(), style));
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        style.PerspectiveOriginY(), style));
  }
  return list;
}

}  // namespace CSSLonghand
}  // namespace blink
