// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyLengthUtils.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/frame/UseCounter.h"

namespace blink {

namespace {

bool validWidthOrHeightKeyword(CSSValueID id, const CSSParserContext* context) {
  if (id == CSSValueWebkitMinContent || id == CSSValueWebkitMaxContent ||
      id == CSSValueWebkitFillAvailable || id == CSSValueWebkitFitContent ||
      id == CSSValueMinContent || id == CSSValueMaxContent ||
      id == CSSValueFitContent) {
    switch (id) {
      case CSSValueWebkitMinContent:
        context->count(UseCounter::CSSValuePrefixedMinContent);
        break;
      case CSSValueWebkitMaxContent:
        context->count(UseCounter::CSSValuePrefixedMaxContent);
        break;
      case CSSValueWebkitFillAvailable:
        context->count(UseCounter::CSSValuePrefixedFillAvailable);
        break;
      case CSSValueWebkitFitContent:
        context->count(UseCounter::CSSValuePrefixedFitContent);
        break;
      default:
        break;
    }
    return true;
  }
  return false;
}

}  // namespace

CSSValue* CSSPropertyLengthUtils::consumeMaxWidthOrHeight(
    CSSParserTokenRange& range,
    const CSSParserContext* context,
    CSSPropertyParserHelpers::UnitlessQuirk unitless) {
  if (range.peek().id() == CSSValueNone ||
      validWidthOrHeightKeyword(range.peek().id(), context))
    return CSSPropertyParserHelpers::consumeIdent(range);
  return CSSPropertyParserHelpers::consumeLengthOrPercent(
      range, context->mode(), ValueRangeNonNegative, unitless);
}

CSSValue* CSSPropertyLengthUtils::consumeWidthOrHeight(
    CSSParserTokenRange& range,
    const CSSParserContext* context,
    CSSPropertyParserHelpers::UnitlessQuirk unitless) {
  if (range.peek().id() == CSSValueAuto ||
      validWidthOrHeightKeyword(range.peek().id(), context))
    return CSSPropertyParserHelpers::consumeIdent(range);
  return CSSPropertyParserHelpers::consumeLengthOrPercent(
      range, context->mode(), ValueRangeNonNegative, unitless);
}

}  // namespace blink
