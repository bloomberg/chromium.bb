// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIScale.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

const CSSValue* CSSPropertyAPIScale::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  DCHECK(RuntimeEnabledFeatures::cssIndependentTransformPropertiesEnabled());

  CSSValueID id = range.peek().id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::consumeIdent(range);

  CSSValue* scale =
      CSSPropertyParserHelpers::consumeNumber(range, ValueRangeAll);
  if (!scale)
    return nullptr;
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  list->append(*scale);
  scale = CSSPropertyParserHelpers::consumeNumber(range, ValueRangeAll);
  if (scale) {
    list->append(*scale);
    scale = CSSPropertyParserHelpers::consumeNumber(range, ValueRangeAll);
    if (scale)
      list->append(*scale);
  }

  return list;
}

}  // namespace blink
