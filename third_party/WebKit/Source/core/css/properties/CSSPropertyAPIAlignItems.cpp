// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIAlignItems.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyAlignmentUtils.h"
#include "platform/RuntimeEnabledFeatures.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIAlignItems::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
  // align-items property does not allow the 'auto' value.
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueAuto>(range.Peek().Id()))
    return nullptr;
  return CSSPropertyAlignmentUtils::ConsumeSelfPositionOverflowPosition(range);
}

}  // namespace blink
