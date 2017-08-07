// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIBorderImageOutset.h"

#include "core/css/properties/CSSPropertyBorderImageUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPIBorderImageOutset::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyBorderImageUtils::ConsumeBorderImageOutset(range);
}

}  // namespace blink
