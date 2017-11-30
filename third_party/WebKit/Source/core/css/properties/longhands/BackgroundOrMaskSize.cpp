// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/BackgroundOrMaskSize.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* BackgroundOrMaskSize::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSParsingUtils::ConsumeBackgroundSize, range, context.Mode(),
      local_context.UseAliasParsing()
          ? CSSParsingUtils::ParsingStyle::kLegacy
          : CSSParsingUtils::ParsingStyle::kNotLegacy);
}

}  // namespace CSSLonghand
}  // namespace blink
