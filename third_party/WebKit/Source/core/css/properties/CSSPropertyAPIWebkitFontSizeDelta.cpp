// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWebkitFontSizeDelta.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIWebkitFontSizeDelta::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  return CSSPropertyParserHelpers::consumeLength(
      range, context->mode(), ValueRangeAll,
      CSSPropertyParserHelpers::UnitlessQuirk::Allow);
}

}  // namespace blink
