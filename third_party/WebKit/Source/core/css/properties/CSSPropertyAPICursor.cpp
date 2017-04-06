// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPICursor.h"

#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/frame/UseCounter.h"

namespace blink {

const CSSValue* CSSPropertyAPICursor::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  bool inQuirksMode = isQuirksModeBehavior(context->mode());
  CSSValueList* list = nullptr;
  while (CSSValue* image = CSSPropertyParserHelpers::consumeImage(
             range, context,
             CSSPropertyParserHelpers::ConsumeGeneratedImagePolicy::Forbid)) {
    double num;
    IntPoint hotSpot(-1, -1);
    bool hotSpotSpecified = false;
    if (CSSPropertyParserHelpers::consumeNumberRaw(range, num)) {
      hotSpot.setX(int(num));
      if (!CSSPropertyParserHelpers::consumeNumberRaw(range, num))
        return nullptr;
      hotSpot.setY(int(num));
      hotSpotSpecified = true;
    }

    if (!list)
      list = CSSValueList::createCommaSeparated();

    list->append(
        *CSSCursorImageValue::create(*image, hotSpotSpecified, hotSpot));
    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(range))
      return nullptr;
  }

  CSSValueID id = range.peek().id();
  if (!range.atEnd()) {
    if (id == CSSValueWebkitZoomIn)
      context->count(UseCounter::PrefixedCursorZoomIn);
    else if (id == CSSValueWebkitZoomOut)
      context->count(UseCounter::PrefixedCursorZoomOut);
  }
  CSSValue* cursorType = nullptr;
  if (id == CSSValueHand) {
    if (!inQuirksMode)  // Non-standard behavior
      return nullptr;
    cursorType = CSSIdentifierValue::create(CSSValuePointer);
    range.consumeIncludingWhitespace();
  } else if ((id >= CSSValueAuto && id <= CSSValueWebkitZoomOut) ||
             id == CSSValueCopy || id == CSSValueNone) {
    cursorType = CSSPropertyParserHelpers::consumeIdent(range);
  } else {
    return nullptr;
  }

  if (!list)
    return cursorType;
  list->append(*cursorType);
  return list;
}

}  // namespace blink
