// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPILogicalWidthOrHeight.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyLengthUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPILogicalWidthOrHeight::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyLengthUtils::ConsumeWidthOrHeight(range, context);
}

}  // namespace blink
