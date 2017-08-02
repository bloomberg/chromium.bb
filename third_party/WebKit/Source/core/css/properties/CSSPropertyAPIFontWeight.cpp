// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIFontWeight.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSPropertyFontUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPIFontWeight::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  return CSSPropertyFontUtils::ConsumeFontWeight(range, context.Mode());
}

}  // namespace blink
