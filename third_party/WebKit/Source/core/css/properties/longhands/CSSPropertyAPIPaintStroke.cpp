// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CSSPropertyAPIPaintStroke.h"

#include "core/css/CSSURIValue.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIPaintStroke::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  CSSURIValue* url = CSSPropertyParserHelpers::ConsumeUrl(range, &context);
  if (url) {
    CSSValue* parsed_value = nullptr;
    if (range.Peek().Id() == CSSValueNone) {
      parsed_value = CSSPropertyParserHelpers::ConsumeIdent(range);
    } else {
      parsed_value =
          CSSPropertyParserHelpers::ConsumeColor(range, context.Mode());
    }
    if (parsed_value) {
      CSSValueList* values = CSSValueList::CreateSpaceSeparated();
      values->Append(*url);
      values->Append(*parsed_value);
      return values;
    }
    return url;
  }
  return CSSPropertyParserHelpers::ConsumeColor(range, context.Mode());
}

}  // namespace blink
