// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIOutlineColor.h"

#include "core/CSSValueKeywords.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIOutlineColor::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  // Allow the special focus color even in HTML Standard parsing mode.
  if (range.peek().id() == CSSValueWebkitFocusRingColor)
    return CSSPropertyParserHelpers::consumeIdent(range);
  return CSSPropertyParserHelpers::consumeColor(range, context->mode());
}

}  // namespace blink
