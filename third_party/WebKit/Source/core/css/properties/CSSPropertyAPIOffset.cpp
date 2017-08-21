// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIOffset.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyMarginUtils.h"

namespace blink {

class CSSParserLocalContext;

const CSSValue* CSSPropertyAPIOffset::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyMarginUtils::ConsumeMarginOrOffset(
      range, context.Mode(), CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

}  // namespace blink
