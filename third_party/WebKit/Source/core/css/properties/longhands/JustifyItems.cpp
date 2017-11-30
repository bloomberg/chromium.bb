// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/JustifyItems.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* JustifyItems::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
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
  return CSSParsingUtils::ConsumeSelfPositionOverflowPosition(range);
}

}  // namespace CSSLonghand
}  // namespace blink
