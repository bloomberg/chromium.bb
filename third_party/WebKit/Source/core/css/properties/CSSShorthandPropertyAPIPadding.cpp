// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIPadding.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

bool CSSShorthandPropertyAPIPadding::ParseShorthand(
    CSSPropertyID,
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    bool,
    HeapVector<CSSProperty, 256>& properties) const {
  return CSSPropertyParserHelpers::ConsumeShorthandVia4LonghandAPIs(
      paddingShorthand(), important, context, range, properties);
}

}  // namespace blink
