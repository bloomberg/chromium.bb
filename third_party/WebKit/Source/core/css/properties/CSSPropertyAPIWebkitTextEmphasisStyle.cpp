// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWebkitTextEmphasisStyle.h"

#include "core/css/CSSStringValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIWebkitTextEmphasisStyle::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  if (CSSValue* text_emphasis_style =
          CSSPropertyParserHelpers::ConsumeString(range))
    return text_emphasis_style;

  CSSIdentifierValue* fill =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueFilled, CSSValueOpen>(
          range);
  CSSIdentifierValue* shape =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueDot, CSSValueCircle,
                                             CSSValueDoubleCircle,
                                             CSSValueTriangle, CSSValueSesame>(
          range);
  if (!fill) {
    fill = CSSPropertyParserHelpers::ConsumeIdent<CSSValueFilled, CSSValueOpen>(
        range);
  }
  if (fill && shape) {
    CSSValueList* parsed_values = CSSValueList::CreateSpaceSeparated();
    parsed_values->Append(*fill);
    parsed_values->Append(*shape);
    return parsed_values;
  }
  if (fill)
    return fill;
  if (shape)
    return shape;
  return nullptr;
}

}  // namespace blink
