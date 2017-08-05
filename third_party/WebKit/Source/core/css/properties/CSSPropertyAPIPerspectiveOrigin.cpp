// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIPerspectiveOrigin.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIPerspectiveOrigin::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  return ConsumePosition(range, context,
                         CSSPropertyParserHelpers::UnitlessQuirk::kForbid,
                         WebFeature::kThreeValuedPositionPerspectiveOrigin);
}

}  // namespace blink
