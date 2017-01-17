// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPITextIndent.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

const CSSValue* CSSPropertyAPITextIndent::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  // [ <length> | <percentage> ] && hanging? && each-line?
  // Keywords only allowed when css3Text is enabled.
  CSSValueList* list = CSSValueList::createSpaceSeparated();

  bool hasLengthOrPercentage = false;
  bool hasEachLine = false;
  bool hasHanging = false;

  do {
    if (!hasLengthOrPercentage) {
      if (CSSValue* textIndent =
              CSSPropertyParserHelpers::consumeLengthOrPercent(
                  range, context->mode(), ValueRangeAll,
                  CSSPropertyParserHelpers::UnitlessQuirk::Allow)) {
        list->append(*textIndent);
        hasLengthOrPercentage = true;
        continue;
      }
    }

    if (RuntimeEnabledFeatures::css3TextEnabled()) {
      CSSValueID id = range.peek().id();
      if (!hasEachLine && id == CSSValueEachLine) {
        list->append(*CSSPropertyParserHelpers::consumeIdent(range));
        hasEachLine = true;
        continue;
      }
      if (!hasHanging && id == CSSValueHanging) {
        list->append(*CSSPropertyParserHelpers::consumeIdent(range));
        hasHanging = true;
        continue;
      }
    }
    return nullptr;
  } while (!range.atEnd());

  if (!hasLengthOrPercentage)
    return nullptr;

  return list;
}

}  // namespace blink
