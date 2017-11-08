// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/WebkitOriginX.h"

#include "core/CSSValueKeywords.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSPropertyPositionUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* WebkitOriginX::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyPositionUtils::ConsumePositionLonghand<CSSValueLeft,
                                                           CSSValueRight>(
      range, context.Mode());
}

}  // namespace CSSLonghand
}  // namespace blink
