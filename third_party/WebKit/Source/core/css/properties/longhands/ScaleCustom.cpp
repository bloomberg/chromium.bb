// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/Scale.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/style/ComputedStyle.h"
#include "platform/runtime_enabled_features.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* Scale::ParseSingleValue(CSSParserTokenRange& range,
                                        const CSSParserContext& context,
                                        const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSIndependentTransformPropertiesEnabled());

  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValue* scale =
      CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeAll);
  if (!scale)
    return nullptr;
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*scale);
  scale = CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeAll);
  if (scale) {
    list->Append(*scale);
    scale = CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeAll);
    if (scale)
      list->Append(*scale);
  }

  return list;
}

const CSSValue* Scale::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  if (!style.Scale())
    return CSSIdentifierValue::Create(CSSValueNone);
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*CSSPrimitiveValue::Create(
      style.Scale()->X(), CSSPrimitiveValue::UnitType::kNumber));
  if (style.Scale()->Y() == 1 && style.Scale()->Z() == 1)
    return list;
  list->Append(*CSSPrimitiveValue::Create(
      style.Scale()->Y(), CSSPrimitiveValue::UnitType::kNumber));
  if (style.Scale()->Z() != 1) {
    list->Append(*CSSPrimitiveValue::Create(
        style.Scale()->Z(), CSSPrimitiveValue::UnitType::kNumber));
  }
  return list;
}

}  // namespace CSSLonghand
}  // namespace blink
