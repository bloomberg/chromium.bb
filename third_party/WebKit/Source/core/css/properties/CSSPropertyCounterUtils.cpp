// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyCounterUtils.h"

#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

CSSValue* CSSPropertyCounterUtils::consumeCounter(CSSParserTokenRange& range,
                                                  int defaultValue) {
  if (range.peek().id() == CSSValueNone)
    return CSSPropertyParserHelpers::consumeIdent(range);

  CSSValueList* list = CSSValueList::createSpaceSeparated();
  do {
    CSSCustomIdentValue* counterName =
        CSSPropertyParserHelpers::consumeCustomIdent(range);
    if (!counterName)
      return nullptr;
    int value = defaultValue;
    if (CSSPrimitiveValue* counterValue =
            CSSPropertyParserHelpers::consumeInteger(range))
      value = clampTo<int>(counterValue->getDoubleValue());
    list->append(*CSSValuePair::create(
        counterName,
        CSSPrimitiveValue::create(value, CSSPrimitiveValue::UnitType::Integer),
        CSSValuePair::DropIdenticalValues));
  } while (!range.atEnd());
  return list;
}

}  // namespace blink
