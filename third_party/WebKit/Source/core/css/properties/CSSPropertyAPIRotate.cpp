// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIRotate.h"

#include "core/CSSValueKeywords.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIRotate::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  DCHECK(RuntimeEnabledFeatures::CSSIndependentTransformPropertiesEnabled());

  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  for (unsigned i = 0; i < 3; i++) {  // 3 dimensions of rotation
    CSSValue* dimension =
        CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeAll);
    if (!dimension) {
      if (i == 0)
        break;
      return nullptr;
    }
    list->Append(*dimension);
  }

  CSSValue* rotation = CSSPropertyParserHelpers::ConsumeAngle(
      range, &context, WTF::Optional<WebFeature>());
  if (!rotation)
    return nullptr;
  list->Append(*rotation);

  return list;
}

}  // namespace blink
