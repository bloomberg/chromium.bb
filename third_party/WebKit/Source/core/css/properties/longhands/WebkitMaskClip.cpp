// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/WebkitMaskClip.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyBackgroundUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* WebkitMaskClip::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyBackgroundUtils::ConsumePrefixedBackgroundBox, range,
      AllowTextValue::kAllow);
}

}  // namespace CSSLonghand
}  // namespace blink
