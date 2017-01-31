// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIJustifyItems.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyAlignmentUtils.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

const CSSValue* CSSPropertyAPIJustifyItems::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  DCHECK(RuntimeEnabledFeatures::cssGridLayoutEnabled());
  CSSParserTokenRange rangeCopy = range;
  CSSIdentifierValue* legacy =
      CSSPropertyParserHelpers::consumeIdent<CSSValueLegacy>(rangeCopy);
  CSSIdentifierValue* positionKeyword =
      CSSPropertyParserHelpers::consumeIdent<CSSValueCenter, CSSValueLeft,
                                             CSSValueRight>(rangeCopy);
  if (!legacy)
    legacy = CSSPropertyParserHelpers::consumeIdent<CSSValueLegacy>(rangeCopy);
  if (legacy && positionKeyword) {
    range = rangeCopy;
    return CSSValuePair::create(legacy, positionKeyword,
                                CSSValuePair::DropIdenticalValues);
  }
  return CSSPropertyAlignmentUtils::consumeSelfPositionOverflowPosition(range);
}

}  // namespace blink
