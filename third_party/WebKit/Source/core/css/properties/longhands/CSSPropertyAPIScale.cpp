// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CSSPropertyAPIScale.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

const CSSValue* CSSPropertyAPIScale::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSIndependentTransformPropertiesEnabled());

  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValue* scale =
      CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeAll);
  if (!scale)
    return nullptr;
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*scale);
  scale = CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeAll);
  if (scale) {
    list->Append(*scale);
    scale = CSSPropertyParserHelpers::ConsumeNumber(range, kValueRangeAll);
    if (scale)
      list->Append(*scale);
  }

  return list;
}

}  // namespace blink
