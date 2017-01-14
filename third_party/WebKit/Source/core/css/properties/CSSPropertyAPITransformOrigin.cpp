// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPITransformOrigin.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPITransformOrigin::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSValue* resultX = nullptr;
  CSSValue* resultY = nullptr;
  if (CSSPropertyParserHelpers::consumeOneOrTwoValuedPosition(
          range, context->mode(),
          CSSPropertyParserHelpers::UnitlessQuirk::Forbid, resultX, resultY)) {
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    list->append(*resultX);
    list->append(*resultY);
    CSSValue* resultZ = CSSPropertyParserHelpers::consumeLength(
        range, context->mode(), ValueRangeAll);
    if (!resultZ) {
      resultZ =
          CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Pixels);
    }
    list->append(*resultZ);
    return list;
  }
  return nullptr;
}

}  // namespace blink
