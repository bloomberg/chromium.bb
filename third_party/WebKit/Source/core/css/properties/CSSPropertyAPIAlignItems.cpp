// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIAlignItems.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyAlignmentUtils.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

const CSSValue* CSSPropertyAPIAlignItems::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  DCHECK(RuntimeEnabledFeatures::cssGridLayoutEnabled());
  // align-items property does not allow the 'auto' value.
  if (CSSPropertyParserHelpers::identMatches<CSSValueAuto>(range.peek().id()))
    return nullptr;
  return CSSPropertyAlignmentUtils::consumeSelfPositionOverflowPosition(range);
}

}  // namespace blink
