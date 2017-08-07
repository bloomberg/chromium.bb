// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIAlignOrJustifyContent.h"

#include "core/css/properties/CSSPropertyAlignmentUtils.h"
#include "platform/RuntimeEnabledFeatures.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIAlignOrJustifyContent::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
  return CSSPropertyAlignmentUtils::ConsumeContentDistributionOverflowPosition(
      range);
}

}  // namespace blink
