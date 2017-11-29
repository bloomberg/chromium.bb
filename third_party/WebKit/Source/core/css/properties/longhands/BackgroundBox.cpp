// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/BackgroundBox.h"

#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyBackgroundUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* BackgroundBox::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext& local_context) const {
  // This is legacy behavior that does not match spec, see crbug.com/604023
  if (local_context.UseAliasParsing()) {
    return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
        CSSPropertyBackgroundUtils::ConsumePrefixedBackgroundBox, range,
        AllowTextValue::kAllow);
  }
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyBackgroundUtils::ConsumeBackgroundBox, range);
}

}  // namespace CSSLonghand
}  // namespace blink
