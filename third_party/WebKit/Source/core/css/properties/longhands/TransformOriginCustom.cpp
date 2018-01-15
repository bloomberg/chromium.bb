// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/TransformOrigin.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* TransformOrigin::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValue* result_x = nullptr;
  CSSValue* result_y = nullptr;
  if (CSSPropertyParserHelpers::ConsumeOneOrTwoValuedPosition(
          range, context.Mode(),
          CSSPropertyParserHelpers::UnitlessQuirk::kForbid, result_x,
          result_y)) {
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    list->Append(*result_x);
    list->Append(*result_y);
    CSSValue* result_z = CSSPropertyParserHelpers::ConsumeLength(
        range, context.Mode(), kValueRangeAll);
    if (!result_z) {
      result_z =
          CSSPrimitiveValue::Create(0, CSSPrimitiveValue::UnitType::kPixels);
    }
    list->Append(*result_z);
    return list;
  }
  return nullptr;
}

bool TransformOrigin::IsLayoutDependent(const ComputedStyle* style,
                                        LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* TransformOrigin::CSSValueFromComputedStyleInternal(
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
        MinimumValueForLength(style.TransformOriginX(), box.Width()), style));
    list->Append(*ZoomAdjustedPixelValue(
        MinimumValueForLength(style.TransformOriginY(), box.Height()), style));
    if (style.TransformOriginZ() != 0)
      list->Append(*ZoomAdjustedPixelValue(style.TransformOriginZ(), style));
  } else {
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        style.TransformOriginX(), style));
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        style.TransformOriginY(), style));
    if (style.TransformOriginZ() != 0)
      list->Append(*ZoomAdjustedPixelValue(style.TransformOriginZ(), style));
  }
  return list;
}

}  // namespace CSSLonghand
}  // namespace blink
