// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWebkitMaxLogicalWidthOrHeight.h"

#include "core/css/properties/CSSPropertyLengthUtils.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIWebkitMaxLogicalWidthOrHeight::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyLengthUtils::ConsumeMaxWidthOrHeight(range, context);
}

}  // namespace blink
