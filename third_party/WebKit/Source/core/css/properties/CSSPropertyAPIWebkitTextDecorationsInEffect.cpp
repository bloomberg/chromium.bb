// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWebkitTextDecorationsInEffect.h"

#include "core/css/properties/CSSPropertyTextDecorationLineUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPIWebkitTextDecorationsInEffect::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) {
  return CSSPropertyTextDecorationLineUtils::ConsumeTextDecorationLine(range);
}

}  // namespace blink
