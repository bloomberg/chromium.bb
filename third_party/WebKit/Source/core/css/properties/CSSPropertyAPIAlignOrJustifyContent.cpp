// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIAlignOrJustifyContent.h"

#include "core/css/properties/CSSPropertyAlignmentUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPIAlignOrJustifyContent::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyAlignmentUtils::ConsumeContentDistributionOverflowPosition(
      range);
}

}  // namespace blink
