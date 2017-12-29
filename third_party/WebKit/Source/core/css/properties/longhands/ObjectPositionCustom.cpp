// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/ObjectPosition.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/frame/WebFeature.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* ObjectPosition::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumePosition(range, context,
                         CSSPropertyParserHelpers::UnitlessQuirk::kForbid,
                         WebFeature::kThreeValuedPositionObjectPosition);
}

}  // namespace CSSLonghand
}  // namespace blink
