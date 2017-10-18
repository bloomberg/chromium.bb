// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CSSPropertyAPIWebkitTextEmphasisPosition.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

class CSSParserLocalContext;
namespace blink {

// [ over | under ] && [ right | left ]?
// If [ right | left ] is omitted, it defaults to right.
const CSSValue* CSSPropertyAPIWebkitTextEmphasisPosition::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSIdentifierValue* values[2] = {
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueOver, CSSValueUnder,
                                             CSSValueRight, CSSValueLeft>(
          range),
      nullptr};
  if (!values[0])
    return nullptr;
  values[1] = CSSPropertyParserHelpers::ConsumeIdent<
      CSSValueOver, CSSValueUnder, CSSValueRight, CSSValueLeft>(range);
  CSSIdentifierValue* over_under = nullptr;
  CSSIdentifierValue* left_right = nullptr;

  for (auto value : values) {
    if (!value)
      break;
    switch (value->GetValueID()) {
      case CSSValueOver:
      case CSSValueUnder:
        if (over_under)
          return nullptr;
        over_under = value;
        break;
      case CSSValueLeft:
      case CSSValueRight:
        if (left_right)
          return nullptr;
        left_right = value;
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  if (!over_under)
    return nullptr;
  if (!left_right)
    left_right = CSSIdentifierValue::Create(CSSValueRight);
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*over_under);
  list->Append(*left_right);
  return list;
}

}  // namespace blink
