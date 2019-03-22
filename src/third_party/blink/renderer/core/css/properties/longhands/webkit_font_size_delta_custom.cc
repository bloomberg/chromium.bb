// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/webkit_font_size_delta.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"

namespace blink {
namespace css_longhand {

const CSSValue* WebkitFontSizeDelta::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_property_parser_helpers::ConsumeLength(
      range, context.Mode(), kValueRangeAll,
      css_property_parser_helpers::UnitlessQuirk::kAllow);
}

}  // namespace css_longhand
}  // namespace blink
