// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CSSPropertyAPIBorderWidth.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyWebkitBorderWidthUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPIBorderWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  CSSPropertyID shorthand = local_context.CurrentShorthand();
  bool allow_quirky_lengths =
      IsQuirksModeBehavior(context.Mode()) &&
      (shorthand == CSSPropertyInvalid || shorthand == CSSPropertyBorderWidth);
  CSSPropertyParserHelpers::UnitlessQuirk unitless =
      allow_quirky_lengths ? CSSPropertyParserHelpers::UnitlessQuirk::kAllow
                           : CSSPropertyParserHelpers::UnitlessQuirk::kForbid;
  return CSSPropertyWebkitBorderWidthUtils::ConsumeBorderWidth(
      range, context.Mode(), unitless);
}

}  // namespace blink
