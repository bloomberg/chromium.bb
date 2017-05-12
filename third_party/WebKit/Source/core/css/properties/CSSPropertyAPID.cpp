// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPID.h"

#include "core/css/properties/CSSPropertyOffsetPathUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPID::parseSingleValue(CSSParserTokenRange& range,
                                                  const CSSParserContext&,
                                                  CSSPropertyID) {
  return CSSPropertyOffsetPathUtils::ConsumePathOrNone(range);
}

}  // namespace blink
