// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPITransformOrigin.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPITransformOrigin::ParseSingleValue(
    CSSPropertyID,
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

}  // namespace blink
