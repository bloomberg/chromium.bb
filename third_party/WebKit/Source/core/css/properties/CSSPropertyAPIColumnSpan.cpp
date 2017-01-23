// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIColumnSpan.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIColumnSpan::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  return CSSPropertyParserHelpers::consumeIdent<CSSValueAll, CSSValueNone>(
      range);
}

}  // namespace blink
