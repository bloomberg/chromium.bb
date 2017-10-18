// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CSSPropertyAPID.h"

#include "core/css/properties/CSSPropertyOffsetPathUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPID::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyOffsetPathUtils::ConsumePathOrNone(range);
}

}  // namespace blink
