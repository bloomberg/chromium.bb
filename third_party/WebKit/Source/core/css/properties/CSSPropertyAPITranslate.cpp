// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPITranslate.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPITranslate::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  DCHECK(RuntimeEnabledFeatures::cssIndependentTransformPropertiesEnabled());
  CSSValueID id = range.peek().id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::consumeIdent(range);

  CSSValue* translate = CSSPropertyParserHelpers::consumeLengthOrPercent(
      range, context->mode(), ValueRangeAll);
  if (!translate)
    return nullptr;
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  list->append(*translate);
  translate = CSSPropertyParserHelpers::consumeLengthOrPercent(
      range, context->mode(), ValueRangeAll);
  if (translate) {
    list->append(*translate);
    translate = CSSPropertyParserHelpers::consumeLength(range, context->mode(),
                                                        ValueRangeAll);
    if (translate)
      list->append(*translate);
  }

  return list;
}

}  // namespace blink
