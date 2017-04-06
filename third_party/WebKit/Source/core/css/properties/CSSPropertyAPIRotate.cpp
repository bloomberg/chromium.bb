// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIRotate.h"

#include "core/CSSValueKeywords.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

const CSSValue* CSSPropertyAPIRotate::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  DCHECK(RuntimeEnabledFeatures::cssIndependentTransformPropertiesEnabled());

  CSSValueID id = range.peek().id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::consumeIdent(range);

  CSSValueList* list = CSSValueList::createSpaceSeparated();

  for (unsigned i = 0; i < 3; i++) {  // 3 dimensions of rotation
    CSSValue* dimension =
        CSSPropertyParserHelpers::consumeNumber(range, ValueRangeAll);
    if (!dimension) {
      if (i == 0)
        break;
      return nullptr;
    }
    list->append(*dimension);
  }

  CSSValue* rotation = CSSPropertyParserHelpers::consumeAngle(range);
  if (!rotation)
    return nullptr;
  list->append(*rotation);

  return list;
}

}  // namespace blink
