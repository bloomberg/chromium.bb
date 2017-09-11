// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWebkitOriginY.h"

#include "core/CSSValueKeywords.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSPropertyPositionUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPIWebkitOriginY::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyPositionUtils::ConsumePositionLonghand<CSSValueTop,
                                                           CSSValueBottom>(
      range, context.Mode());
}

}  // namespace blink
