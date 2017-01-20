// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIShapeOutside.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyShapeUtils.h"

namespace blink {

using namespace CSSPropertyParserHelpers;

const CSSValue* CSSPropertyAPIShapeOutside::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  if (CSSValue* imageValue = consumeImageOrNone(range, context))
    return imageValue;
  CSSValueList* list = CSSValueList::createSpaceSeparated();
  if (CSSValue* boxValue = consumeShapeBox(range))
    list->append(*boxValue);
  if (CSSValue* shapeValue =
          CSSPropertyShapeUtils::consumeBasicShape(range, context)) {
    list->append(*shapeValue);
    if (list->length() < 2) {
      if (CSSValue* boxValue = consumeShapeBox(range))
        list->append(*boxValue);
    }
  }
  if (!list->length())
    return nullptr;
  return list;
}

}  // namespace blink
