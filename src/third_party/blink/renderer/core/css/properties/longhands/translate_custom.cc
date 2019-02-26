// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/translate.h"

#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

const CSSValue* Translate::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSIndependentTransformPropertiesEnabled());
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNone)
    return css_property_parser_helpers::ConsumeIdent(range);

  CSSValue* translate_x = css_property_parser_helpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeAll);
  if (!translate_x)
    return nullptr;
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*translate_x);
  CSSPrimitiveValue* translate_y =
      css_property_parser_helpers::ConsumeLengthOrPercent(range, context.Mode(),
                                                          kValueRangeAll);
  if (translate_y) {
    CSSValue* translate_z = css_property_parser_helpers::ConsumeLength(
        range, context.Mode(), kValueRangeAll);
    if (translate_y->GetIntValue() == 0 && !translate_z)
      return list;

    list->Append(*translate_y);
    if (translate_z) {
      list->Append(*translate_z);
    }
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
  list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.Translate()->X(), style));

  if (!style.Translate()->Y().IsZero() || style.Translate()->Z() != 0) {
    list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
        style.Translate()->Y(), style));
  }

  if (style.Translate()->Z() != 0)
    list->Append(*ZoomAdjustedPixelValue(style.Translate()->Z(), style));

  return list;
}

}  // namespace css_longhand
}  // namespace blink
