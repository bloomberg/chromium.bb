// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIPaintOrder.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIPaintOrder::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  if (range.peek().id() == CSSValueNormal)
    return CSSPropertyParserHelpers::consumeIdent(range);

  Vector<CSSValueID, 3> paintTypeList;
  CSSIdentifierValue* fill = nullptr;
  CSSIdentifierValue* stroke = nullptr;
  CSSIdentifierValue* markers = nullptr;
  do {
    CSSValueID id = range.peek().id();
    if (id == CSSValueFill && !fill)
      fill = CSSPropertyParserHelpers::consumeIdent(range);
    else if (id == CSSValueStroke && !stroke)
      stroke = CSSPropertyParserHelpers::consumeIdent(range);
    else if (id == CSSValueMarkers && !markers)
      markers = CSSPropertyParserHelpers::consumeIdent(range);
    else
      return nullptr;
    paintTypeList.push_back(id);
  } while (!range.atEnd());

  // After parsing we serialize the paint-order list. Since it is not possible
  // to pop a last list items from CSSValueList without bigger cost, we create
  // the list after parsing.
  CSSValueID firstPaintOrderType = paintTypeList.at(0);
  CSSValueList* paintOrderList = CSSValueList::createSpaceSeparated();
  switch (firstPaintOrderType) {
    case CSSValueFill:
    case CSSValueStroke:
      paintOrderList->append(firstPaintOrderType == CSSValueFill ? *fill
                                                                 : *stroke);
      if (paintTypeList.size() > 1) {
        if (paintTypeList.at(1) == CSSValueMarkers)
          paintOrderList->append(*markers);
      }
      break;
    case CSSValueMarkers:
      paintOrderList->append(*markers);
      if (paintTypeList.size() > 1) {
        if (paintTypeList.at(1) == CSSValueStroke)
          paintOrderList->append(*stroke);
      }
      break;
    default:
      NOTREACHED();
  }

  return paintOrderList;
}

}  // namespace blink
