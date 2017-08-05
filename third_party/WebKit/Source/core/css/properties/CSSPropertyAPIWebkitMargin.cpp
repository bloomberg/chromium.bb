// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWebkitMargin.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyMarginUtils.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIWebkitMargin::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  return CSSPropertyMarginUtils::ConsumeMarginOrOffset(
      range, context.Mode(), CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
}

}  // namespace blink
