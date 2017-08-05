// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIAnimationName.h"

#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyAnimationNameUtils.h"

class CSSParserContext;

namespace blink {

const CSSValue* CSSPropertyAPIAnimationName::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) {
  // Allow quoted name if this is an alias property.
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyAnimationNameUtils::ConsumeAnimationName, range, context,
      local_context.UseAliasParsing());
}

}  // namespace blink
