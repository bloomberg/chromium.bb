// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPITransition.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyAnimationTimingFunctionUtils.h"
#include "core/css/properties/CSSPropertyAnimationUtils.h"
#include "core/css/properties/CSSPropertyTransitionPropertyUtils.h"

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
      return CSSPropertyTransitionPropertyUtils::ConsumeTransitionProperty(
          range);
    case CSSPropertyTransitionTimingFunction:
      return CSSPropertyAnimationTimingFunctionUtils::
          ConsumeAnimationTimingFunction(range);
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace

bool CSSShorthandPropertyAPITransition::parseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    bool use_legacy_parsing,
    HeapVector<CSSProperty, 256>& properties) {
  const StylePropertyShorthand shorthand = transitionShorthandForParsing();
  const unsigned longhand_count = shorthand.length();

  HeapVector<CSSValueList*, kMaxNumAnimationLonghands> longhands(
      longhand_count);
  if (!CSSPropertyAnimationUtils::ConsumeAnimationShorthand(
          shorthand, longhands, ConsumeTransitionValue, range, context,
          use_legacy_parsing)) {
    return false;
  }

  for (size_t i = 0; i < longhand_count; ++i) {
    if (shorthand.properties()[i] == CSSPropertyTransitionProperty &&
        !CSSPropertyTransitionPropertyUtils::IsValidPropertyList(*longhands[i]))
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
