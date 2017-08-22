// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIJustifyItems.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyAlignmentUtils.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

const CSSValue* CSSPropertyAPIJustifyItems::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
  CSSParserTokenRange range_copy = range;
  CSSIdentifierValue* legacy =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueLegacy>(range_copy);
  CSSIdentifierValue* position_keyword =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueCenter, CSSValueLeft,
                                             CSSValueRight>(range_copy);
  if (!legacy)
    legacy = CSSPropertyParserHelpers::ConsumeIdent<CSSValueLegacy>(range_copy);
  if (legacy && position_keyword) {
    range = range_copy;
    return CSSValuePair::Create(legacy, position_keyword,
                                CSSValuePair::kDropIdenticalValues);
  }
  return CSSPropertyAlignmentUtils::ConsumeSelfPositionOverflowPosition(range);
}

}  // namespace blink
