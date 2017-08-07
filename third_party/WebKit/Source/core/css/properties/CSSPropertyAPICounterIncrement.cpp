// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPICounterIncrement.h"

#include "core/css/properties/CSSPropertyCounterUtils.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPICounterIncrement::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyCounterUtils::ConsumeCounter(
      range, CSSPropertyCounterUtils::kIncrementDefaultValue);
}

}  // namespace blink
