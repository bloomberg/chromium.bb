// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/scroll_margin_inline_end.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {
namespace css_longhand {

const CSSValue* ScrollMarginInlineEnd::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLength(range, context.Mode(), kValueRangeAll,
                       css_property_parser_helpers::UnitlessQuirk::kForbid);
}

}  // namespace css_longhand
}  // namespace blink
