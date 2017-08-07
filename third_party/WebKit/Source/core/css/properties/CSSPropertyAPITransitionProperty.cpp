// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPITransitionProperty.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyTransitionPropertyUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPITransitionProperty::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) {
  CSSValueList* list = CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyTransitionPropertyUtils::ConsumeTransitionProperty, range);
  if (!list || !CSSPropertyTransitionPropertyUtils::IsValidPropertyList(*list))
    return nullptr;
  return list;
}

}  // namespace blink
