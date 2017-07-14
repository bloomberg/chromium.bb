// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIScrollSnapMarginInline.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

bool CSSShorthandPropertyAPIScrollSnapMarginInline::parseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    bool,
    HeapVector<CSSProperty, 256>& properties) {
  return CSSPropertyParserHelpers::ConsumeShorthandVia2LonghandAPIs(
      scrollSnapMarginInlineShorthand(), important, context, range, properties);
}
}  // namespace blink
