// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIGridLine.h"

#include "core/css/properties/CSSPropertyGridUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPIGridLine::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyGridUtils::ConsumeGridLine(range);
}

}  // namespace blink
