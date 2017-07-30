// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIAnimation.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyAnimationIterationCountUtils.h"
#include "core/css/properties/CSSPropertyAnimationNameUtils.h"
#include "core/css/properties/CSSPropertyAnimationTimingFunctionUtils.h"
#include "core/css/properties/CSSPropertyAnimationUtils.h"

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
      return CSSPropertyAnimationIterationCountUtils::
          ConsumeAnimationIterationCount(range);
    case CSSPropertyAnimationName:
      return CSSPropertyAnimationNameUtils::ConsumeAnimationName(
          range, context, use_legacy_parsing);
    case CSSPropertyAnimationPlayState:
      return CSSPropertyParserHelpers::ConsumeIdent<CSSValueRunning,
                                                    CSSValuePaused>(range);
    case CSSPropertyAnimationTimingFunction:
      return CSSPropertyAnimationTimingFunctionUtils::
          ConsumeAnimationTimingFunction(range);
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace

bool CSSShorthandPropertyAPIAnimation::parseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    bool use_legacy_parsing,
    HeapVector<CSSProperty, 256>& properties) {
  const StylePropertyShorthand shorthand = animationShorthandForParsing();
  const unsigned longhand_count = shorthand.length();

  HeapVector<Member<CSSValueList>, kMaxNumAnimationLonghands> longhands(
      longhand_count);
  if (!CSSPropertyAnimationUtils::ConsumeAnimationShorthand(
          shorthand, longhands, ConsumeAnimationValue, range, context,
          use_legacy_parsing)) {
    return false;
  }

  for (size_t i = 0; i < longhand_count; ++i) {
    CSSPropertyParserHelpers::AddProperty(
        shorthand.properties()[i], shorthand.id(), *longhands[i], important,
        CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  }
  return range.AtEnd();
}

}  // namespace blink
