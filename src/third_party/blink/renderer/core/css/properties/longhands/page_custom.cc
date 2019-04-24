// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/page.h"

#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"

namespace blink {
namespace css_longhand {

const CSSValue* Page::ParseSingleValue(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueAuto)
    return css_property_parser_helpers::ConsumeIdent(range);
  return css_property_parser_helpers::ConsumeCustomIdent(range, context);
}

}  // namespace css_longhand
}  // namespace blink
