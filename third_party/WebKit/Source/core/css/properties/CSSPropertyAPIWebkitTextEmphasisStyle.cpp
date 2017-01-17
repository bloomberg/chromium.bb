// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWebkitTextEmphasisStyle.h"

#include "core/css/CSSStringValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIWebkitTextEmphasisStyle::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSValueID id = range.peek().id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::consumeIdent(range);

  if (CSSValue* textEmphasisStyle =
          CSSPropertyParserHelpers::consumeString(range))
    return textEmphasisStyle;

  CSSIdentifierValue* fill =
      CSSPropertyParserHelpers::consumeIdent<CSSValueFilled, CSSValueOpen>(
          range);
  CSSIdentifierValue* shape =
      CSSPropertyParserHelpers::consumeIdent<CSSValueDot, CSSValueCircle,
                                             CSSValueDoubleCircle,
                                             CSSValueTriangle, CSSValueSesame>(
          range);
  if (!fill) {
    fill = CSSPropertyParserHelpers::consumeIdent<CSSValueFilled, CSSValueOpen>(
        range);
  }
  if (fill && shape) {
    CSSValueList* parsedValues = CSSValueList::createSpaceSeparated();
    parsedValues->append(*fill);
    parsedValues->append(*shape);
    return parsedValues;
  }
  if (fill)
    return fill;
  if (shape)
    return shape;
  return nullptr;
}

}  // namespace blink
