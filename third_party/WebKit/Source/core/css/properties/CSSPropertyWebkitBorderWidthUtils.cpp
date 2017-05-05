// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyWebkitBorderWidthUtils.h"

namespace blink {

CSSValue* CSSPropertyWebkitBorderWidthUtils::ConsumeBorderWidth(
    CSSParserTokenRange& range,
    CSSParserMode css_parser_mode,
    CSSPropertyParserHelpers::UnitlessQuirk unitless) {
  return CSSPropertyParserHelpers::ConsumeLineWidth(range, css_parser_mode,
                                                    unitless);
}

}  // namespace blink
