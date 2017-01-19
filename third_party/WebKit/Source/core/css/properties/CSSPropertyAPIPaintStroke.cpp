// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIPaintStroke.h"

#include "core/css/CSSURIValue.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIPaintStroke::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  if (range.peek().id() == CSSValueNone)
    return CSSPropertyParserHelpers::consumeIdent(range);
  CSSURIValue* url = CSSPropertyParserHelpers::consumeUrl(range, context);
  if (url) {
    CSSValue* parsedValue = nullptr;
    if (range.peek().id() == CSSValueNone) {
      parsedValue = CSSPropertyParserHelpers::consumeIdent(range);
    } else {
      parsedValue =
          CSSPropertyParserHelpers::consumeColor(range, context->mode());
    }
    if (parsedValue) {
      CSSValueList* values = CSSValueList::createSpaceSeparated();
      values->append(*url);
      values->append(*parsedValue);
      return values;
    }
    return url;
  }
  return CSSPropertyParserHelpers::consumeColor(range, context->mode());
}

}  // namespace blink
