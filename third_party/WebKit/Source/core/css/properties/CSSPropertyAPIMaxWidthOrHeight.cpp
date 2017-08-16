// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIMaxWidthOrHeight.h"

#include "core/css/properties/CSSPropertyLengthUtils.h"

namespace blink {
class CSSParserLocalContext;

const CSSValue* CSSPropertyAPIMaxWidthOrHeight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  return CSSPropertyLengthUtils::ConsumeMaxWidthOrHeight(
      range, context, CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

}  // namespace blink
