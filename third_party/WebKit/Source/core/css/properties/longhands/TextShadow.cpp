// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/TextShadow.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSPropertyBoxShadowUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* TextShadow::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyBoxShadowUtils::ConsumeShadow(range, context.Mode(),
                                                  AllowInsetAndSpread::kForbid);
}

}  // namespace CSSLonghand
}  // namespace blink
