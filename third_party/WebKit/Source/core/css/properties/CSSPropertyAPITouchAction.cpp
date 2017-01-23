// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPITouchAction.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

namespace {

static bool consumePan(CSSParserTokenRange& range,
                       CSSValue*& panX,
                       CSSValue*& panY,
                       CSSValue*& pinchZoom) {
  CSSValueID id = range.peek().id();
  if ((id == CSSValuePanX || id == CSSValuePanRight || id == CSSValuePanLeft) &&
      !panX) {
    if (id != CSSValuePanX &&
        !RuntimeEnabledFeatures::cssTouchActionPanDirectionsEnabled())
      return false;
    panX = CSSPropertyParserHelpers::consumeIdent(range);
  } else if ((id == CSSValuePanY || id == CSSValuePanDown ||
              id == CSSValuePanUp) &&
             !panY) {
    if (id != CSSValuePanY &&
        !RuntimeEnabledFeatures::cssTouchActionPanDirectionsEnabled())
      return false;
    panY = CSSPropertyParserHelpers::consumeIdent(range);
  } else if (id == CSSValuePinchZoom && !pinchZoom &&
             RuntimeEnabledFeatures::cssTouchActionPinchZoomEnabled()) {
    pinchZoom = CSSPropertyParserHelpers::consumeIdent(range);
  } else {
    return false;
  }
  return true;
}

}  // namespace

const CSSValue* CSSPropertyAPITouchAction::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  CSSValueID id = range.peek().id();
  if (id == CSSValueAuto || id == CSSValueNone || id == CSSValueManipulation) {
    list->append(*CSSPropertyParserHelpers::consumeIdent(range));
    return list;
  }

  CSSValue* panX = nullptr;
  CSSValue* panY = nullptr;
  CSSValue* pinchZoom = nullptr;
  if (!consumePan(range, panX, panY, pinchZoom))
    return nullptr;
  if (!range.atEnd() && !consumePan(range, panX, panY, pinchZoom))
    return nullptr;
  if (!range.atEnd() && !consumePan(range, panX, panY, pinchZoom))
    return nullptr;

  if (panX)
    list->append(*panX);
  if (panY)
    list->append(*panY);
  if (pinchZoom)
    list->append(*pinchZoom);
  return list;
}

}  // namespace blink
