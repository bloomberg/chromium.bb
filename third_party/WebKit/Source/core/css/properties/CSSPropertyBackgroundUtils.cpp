// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyBackgroundUtils.h"

#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

void CSSPropertyBackgroundUtils::AddBackgroundValue(CSSValue*& list,
                                                    CSSValue* value) {
  if (list) {
    if (!list->IsBaseValueList()) {
      CSSValue* first_value = list;
      list = CSSValueList::CreateCommaSeparated();
      ToCSSValueList(list)->Append(*first_value);
    }
    ToCSSValueList(list)->Append(*value);
  } else {
    // To conserve memory we don't actually wrap a single value in a list.
    list = value;
  }
}

bool CSSPropertyBackgroundUtils::ConsumeBackgroundPosition(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPropertyParserHelpers::UnitlessQuirk unitless,
    CSSValue*& result_x,
    CSSValue*& result_y) {
  do {
    CSSValue* position_x = nullptr;
    CSSValue* position_y = nullptr;
    if (!CSSPropertyParserHelpers::ConsumePosition(
            range, context, unitless,
            WebFeature::kThreeValuedPositionBackground, position_x, position_y))
      return false;
    AddBackgroundValue(result_x, position_x);
    AddBackgroundValue(result_y, position_y);
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));
  return true;
}

}  // namespace blink
