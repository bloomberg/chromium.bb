// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIBoxShadow.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSPropertyBoxShadowUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPIBoxShadow::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  return CSSPropertyBoxShadowUtils::ConsumeShadow(range, context.Mode(),
                                                  AllowInsetAndSpread::kAllow);
}

}  // namespace blink
