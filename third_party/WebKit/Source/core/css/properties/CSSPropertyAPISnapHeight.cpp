// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPISnapHeight.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPISnapHeight::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSPrimitiveValue* unit = CSSPropertyParserHelpers::consumeLength(
      range, context->mode(), ValueRangeNonNegative);
  if (!unit)
    return nullptr;
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  list->append(*unit);

  if (CSSPrimitiveValue* position =
          CSSPropertyParserHelpers::consumePositiveInteger(range)) {
    if (position->getIntValue() > 100)
      return nullptr;
    list->append(*position);
  }

  return list;
}

}  // namespace blink
