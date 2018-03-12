// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/webkit_perspective_origin_y.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css_value_keywords.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* WebkitPerspectiveOriginY::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumePositionLonghand<CSSValueTop, CSSValueBottom>(
      range, context.Mode());
}

}  // namespace CSSLonghand
}  // namespace blink
