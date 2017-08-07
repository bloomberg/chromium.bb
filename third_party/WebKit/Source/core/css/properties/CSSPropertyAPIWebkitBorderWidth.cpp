// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWebkitBorderWidth.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyWebkitBorderWidthUtils.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIWebkitBorderWidth::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyWebkitBorderWidthUtils::ConsumeBorderWidth(
      range, context.Mode(), CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
}

}  // namespace blink
