// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPITextUnderlinePosition.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPITextUnderlinePosition::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  // auto | [ under || [ left | right ] ], but we only support auto | under
  // for now
  DCHECK(RuntimeEnabledFeatures::CSS3TextDecorationsEnabled());
  // auto | [ under || [ left | right ] ], but we only support auto | under
  // for now
  DCHECK(RuntimeEnabledFeatures::CSS3TextDecorationsEnabled());
  return CSSPropertyParserHelpers::ConsumeIdent<CSSValueAuto, CSSValueUnder>(
      range);
}

}  // namespace blink
