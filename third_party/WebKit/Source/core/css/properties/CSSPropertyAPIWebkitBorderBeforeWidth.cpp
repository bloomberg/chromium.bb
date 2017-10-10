// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWebkitBorderBeforeWidth.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSProperty.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyWebkitBorderWidthUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPIWebkitBorderBeforeWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyWebkitBorderWidthUtils::ConsumeBorderWidth(
      range, context.Mode(), CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
}

const CSSPropertyAPI&
CSSPropertyAPIWebkitBorderBeforeWidth::ResolveDirectionAwareProperty(
    TextDirection direction,
    WritingMode writing_mode) const {
  return ResolveToPhysicalPropertyAPI(direction, writing_mode, kBeforeSide,
                                      borderWidthShorthand());
}
}  // namespace blink
