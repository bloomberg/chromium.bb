// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/BorderWidthSide.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* BorderWidthSide::ParseSingleValue(
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
  return CSSParsingUtils::ConsumeBorderWidth(range, context.Mode(), unitless);
}

}  // namespace CSSLonghand
}  // namespace blink
