// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWebkitBorderStartColor.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSProperty.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

class CSSParserLocalContext;

const CSSValue* CSSPropertyAPIWebkitBorderStartColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeColor(range, context.Mode());
}

const CSSPropertyAPI&
CSSPropertyAPIWebkitBorderStartColor::ResolveDirectionAwareProperty(
    TextDirection direction,
    WritingMode writing_mode) const {
  return ResolveToPhysicalPropertyAPI(direction, writing_mode, kStartSide,
                                      borderColorShorthand());
}
}  // namespace blink
