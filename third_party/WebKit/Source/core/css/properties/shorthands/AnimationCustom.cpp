// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/Animation.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"

namespace blink {
namespace {

// Legacy parsing allows <string>s for animation-name.
CSSValue* ConsumeAnimationValue(CSSPropertyID property,
                                CSSParserTokenRange& range,
                                const CSSParserContext& context,
                                bool use_legacy_parsing) {
  switch (property) {
    case CSSPropertyAnimationDelay:
      return CSSPropertyParserHelpers::ConsumeTime(range, kValueRangeAll);
    case CSSPropertyAnimationDirection:
      return CSSPropertyParserHelpers::ConsumeIdent<
          CSSValueNormal, CSSValueAlternate, CSSValueReverse,
          CSSValueAlternateReverse>(range);
    case CSSPropertyAnimationDuration:
      return CSSPropertyParserHelpers::ConsumeTime(range,
                                                   kValueRangeNonNegative);
    case CSSPropertyAnimationFillMode:
      return CSSPropertyParserHelpers::ConsumeIdent<
          CSSValueNone, CSSValueForwards, CSSValueBackwards, CSSValueBoth>(
          range);
    case CSSPropertyAnimationIterationCount:
      return CSSParsingUtils::ConsumeAnimationIterationCount(range);
    case CSSPropertyAnimationName:
      return CSSParsingUtils::ConsumeAnimationName(range, context,
                                                   use_legacy_parsing);
    case CSSPropertyAnimationPlayState:
      return CSSPropertyParserHelpers::ConsumeIdent<CSSValueRunning,
                                                    CSSValuePaused>(range);
    case CSSPropertyAnimationTimingFunction:
      return CSSParsingUtils::ConsumeAnimationTimingFunction(range);
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace
namespace CSSShorthand {

bool Animation::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  const StylePropertyShorthand shorthand = animationShorthandForParsing();
  const unsigned longhand_count = shorthand.length();

  HeapVector<Member<CSSValueList>, CSSParsingUtils::kMaxNumAnimationLonghands>
      longhands(longhand_count);
  if (!CSSParsingUtils::ConsumeAnimationShorthand(
          shorthand, longhands, ConsumeAnimationValue, range, context,
          local_context.UseAliasParsing())) {
    return false;
  }

  for (size_t i = 0; i < longhand_count; ++i) {
    CSSPropertyParserHelpers::AddProperty(
        shorthand.properties()[i]->PropertyID(), shorthand.id(), *longhands[i],
        important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
        properties);
  }
  return range.AtEnd();
}

}  // namespace CSSShorthand
}  // namespace blink
