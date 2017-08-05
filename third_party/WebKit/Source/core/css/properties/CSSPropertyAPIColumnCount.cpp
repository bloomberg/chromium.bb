// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIColumnCount.h"

#include "core/css/properties/CSSPropertyColumnUtils.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIColumnCount::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  return CSSPropertyColumnUtils::ConsumeColumnCount(range);
}

}  // namespace blink
