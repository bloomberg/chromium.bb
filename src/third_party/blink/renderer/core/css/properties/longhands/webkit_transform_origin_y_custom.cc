// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/webkit_transform_origin_y.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"

namespace blink {
namespace css_longhand {

const CSSValue* WebkitTransformOriginY::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumePositionLonghand<CSSValueTop,
                                                    CSSValueBottom>(
      range, context.Mode());
}

}  // namespace css_longhand
}  // namespace blink
