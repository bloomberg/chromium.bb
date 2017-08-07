// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPITextDecoration.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

bool CSSShorthandPropertyAPITextDecoration::ParseShorthand(
    CSSPropertyID,
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    bool,
    HeapVector<CSSProperty, 256>& properties) const {
  DCHECK(RuntimeEnabledFeatures::CSS3TextDecorationsEnabled());
  return CSSPropertyParserHelpers::ConsumeShorthandGreedilyViaLonghandAPIs(
      textDecorationShorthand(), important, context, range, properties);
}
}  // namespace blink
