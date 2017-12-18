// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/Duration.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/Length.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* Duration::ParseSingleValue(CSSParserTokenRange& range,
                                           const CSSParserContext&,
                                           const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyParserHelpers::ConsumeTime, range, kValueRangeNonNegative);
}

}  // namespace CSSLonghand
}  // namespace blink
