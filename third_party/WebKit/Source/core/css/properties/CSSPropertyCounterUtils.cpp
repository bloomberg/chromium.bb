// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyCounterUtils.h"

#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

CSSValue* CSSPropertyCounterUtils::ConsumeCounter(CSSParserTokenRange& range,
                                                  int default_value) {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  do {
    CSSCustomIdentValue* counter_name =
        CSSPropertyParserHelpers::ConsumeCustomIdent(range);
    if (!counter_name)
      return nullptr;
    int value = default_value;
    if (CSSPrimitiveValue* counter_value =
            CSSPropertyParserHelpers::ConsumeInteger(range))
      value = clampTo<int>(counter_value->GetDoubleValue());
    list->Append(*CSSValuePair::Create(
        counter_name,
        CSSPrimitiveValue::Create(value, CSSPrimitiveValue::UnitType::kInteger),
        CSSValuePair::kDropIdenticalValues));
  } while (!range.AtEnd());
  return list;
}

}  // namespace blink
