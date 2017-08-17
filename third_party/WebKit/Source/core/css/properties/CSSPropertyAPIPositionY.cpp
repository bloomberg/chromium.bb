// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIPositionY.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyPositionUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPIPositionY::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyPositionUtils::ConsumePositionLonghand<CSSValueTop,
                                                        CSSValueBottom>,
      range, context.Mode());
}

}  // namespace blink
