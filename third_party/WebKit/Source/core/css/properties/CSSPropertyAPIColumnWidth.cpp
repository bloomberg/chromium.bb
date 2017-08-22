// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIColumnWidth.h"

#include "core/css/properties/CSSPropertyColumnUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPIColumnWidth::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyColumnUtils::ConsumeColumnWidth(range);
}

}  // namespace blink
