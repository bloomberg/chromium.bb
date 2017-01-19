// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIGridAutoFlow.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

const CSSValue* CSSPropertyAPIGridAutoFlow::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  DCHECK(RuntimeEnabledFeatures::cssGridLayoutEnabled());
  CSSIdentifierValue* rowOrColumnValue =
      CSSPropertyParserHelpers::consumeIdent<CSSValueRow, CSSValueColumn>(
          range);
  CSSIdentifierValue* denseAlgorithm =
      CSSPropertyParserHelpers::consumeIdent<CSSValueDense>(range);
  if (!rowOrColumnValue) {
    rowOrColumnValue =
        CSSPropertyParserHelpers::consumeIdent<CSSValueRow, CSSValueColumn>(
            range);
    if (!rowOrColumnValue && !denseAlgorithm)
      return nullptr;
  }
  CSSValueList* parsedValues = CSSValueList::createSpaceSeparated();
  if (rowOrColumnValue)
    parsedValues->append(*rowOrColumnValue);
  if (denseAlgorithm)
    parsedValues->append(*denseAlgorithm);
  return parsedValues;
}

}  // namespace blink
