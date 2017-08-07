// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIBorderColor.h"

#include "core/CSSPropertyNames.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIBorderColor::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  CSSPropertyID shorthand = local_context.CurrentShorthand();
  bool allow_quirky_colors =
      IsQuirksModeBehavior(context.Mode()) &&
      (shorthand == CSSPropertyInvalid || shorthand == CSSPropertyBorderColor);
  return CSSPropertyParserHelpers::ConsumeColor(range, context.Mode(),
                                                allow_quirky_colors);
}

}  // namespace blink
