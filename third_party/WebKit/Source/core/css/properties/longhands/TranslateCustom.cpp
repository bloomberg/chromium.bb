// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/Translate.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* Translate::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSIndependentTransformPropertiesEnabled());
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValue* translate = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeAll);
  if (!translate)
    return nullptr;
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*translate);
  translate = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeAll);
  if (translate) {
    list->Append(*translate);
    translate = CSSPropertyParserHelpers::ConsumeLength(range, context.Mode(),
                                                        kValueRangeAll);
    if (translate)
      list->Append(*translate);
  }

  return list;
}

bool Translate::IsLayoutDependent(const ComputedStyle* style,
                                  LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

const CSSValue* Translate::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  if (!style.Translate())
    return CSSIdentifierValue::Create(CSSValueNone);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (layout_object && layout_object->IsBox()) {
    LayoutRect box = ToLayoutBox(layout_object)->BorderBoxRect();
    list->Append(*ZoomAdjustedPixelValue(
        FloatValueForLength(style.Translate()->X(), box.Width().ToFloat()),
        style));
    if (!style.Translate()->Y().IsZero() || style.Translate()->Z() != 0) {
      list->Append(*ZoomAdjustedPixelValue(
          FloatValueForLength(style.Translate()->Y(), box.Height().ToFloat()),
          style));
    }
  } else {
    // No box to resolve the percentage values
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        style.Translate()->X(), style));

    if (!style.Translate()->Y().IsZero() || style.Translate()->Z() != 0) {
      list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.Translate()->Y(), style));
    }
  }

  if (style.Translate()->Z() != 0)
    list->Append(*ZoomAdjustedPixelValue(style.Translate()->Z(), style));

  return list;
}

}  // namespace CSSLonghand
}  // namespace blink
