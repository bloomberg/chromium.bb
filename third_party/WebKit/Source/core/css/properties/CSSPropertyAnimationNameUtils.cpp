// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAnimationNameUtils.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/frame/UseCounter.h"

namespace blink {

CSSValue* CSSPropertyAnimationNameUtils::ConsumeAnimationName(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    bool allow_quoted_name) {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  if (allow_quoted_name && range.Peek().GetType() == kStringToken) {
    // Legacy support for strings in prefixed animations.
    context.Count(WebFeature::kQuotedAnimationName);

    const CSSParserToken& token = range.ConsumeIncludingWhitespace();
    if (EqualIgnoringASCIICase(token.Value(), "none"))
      return CSSIdentifierValue::Create(CSSValueNone);
    return CSSCustomIdentValue::Create(token.Value().ToAtomicString());
  }

  return CSSPropertyParserHelpers::ConsumeCustomIdent(range);
}

}  // namespace blink
