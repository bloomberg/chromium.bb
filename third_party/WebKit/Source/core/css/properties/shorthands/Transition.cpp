// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/Transition.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"

namespace blink {
namespace {

CSSValue* ConsumeTransitionValue(CSSPropertyID property,
                                 CSSParserTokenRange& range,
                                 const CSSParserContext&,
                                 bool use_legacy_parsing) {
  switch (property) {
    case CSSPropertyTransitionDelay:
      return CSSPropertyParserHelpers::ConsumeTime(range, kValueRangeAll);
    case CSSPropertyTransitionDuration:
      return CSSPropertyParserHelpers::ConsumeTime(range,
                                                   kValueRangeNonNegative);
    case CSSPropertyTransitionProperty:
      return CSSParsingUtils::ConsumeTransitionProperty(range);
    case CSSPropertyTransitionTimingFunction:
      return CSSParsingUtils::ConsumeAnimationTimingFunction(range);
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace
namespace CSSShorthand {

bool Transition::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  const StylePropertyShorthand shorthand = transitionShorthandForParsing();
  const unsigned longhand_count = shorthand.length();

  HeapVector<Member<CSSValueList>, CSSParsingUtils::kMaxNumAnimationLonghands>
      longhands(longhand_count);
  if (!CSSParsingUtils::ConsumeAnimationShorthand(
          shorthand, longhands, ConsumeTransitionValue, range, context,
          local_context.UseAliasParsing())) {
    return false;
  }

  for (size_t i = 0; i < longhand_count; ++i) {
    if (shorthand.properties()[i]->IDEquals(CSSPropertyTransitionProperty) &&
        !CSSParsingUtils::IsValidPropertyList(*longhands[i]))
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
