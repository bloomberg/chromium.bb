// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWebkitHighlight.h"

#include "core/css/CSSStringValue.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIWebkitHighlight::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  if (range.peek().id() == CSSValueNone)
    return CSSPropertyParserHelpers::consumeIdent(range);
  return CSSPropertyParserHelpers::consumeString(range);
}

}  // namespace blink
