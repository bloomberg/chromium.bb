// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyLengthUtils.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/frame/UseCounter.h"

namespace blink {
namespace {

bool ValidWidthOrHeightKeyword(CSSValueID id, const CSSParserContext& context) {
  if (id == CSSValueWebkitMinContent || id == CSSValueWebkitMaxContent ||
      id == CSSValueWebkitFillAvailable || id == CSSValueWebkitFitContent ||
      id == CSSValueMinContent || id == CSSValueMaxContent ||
      id == CSSValueFitContent) {
    switch (id) {
      case CSSValueWebkitMinContent:
        context.Count(WebFeature::kCSSValuePrefixedMinContent);
        break;
      case CSSValueWebkitMaxContent:
        context.Count(WebFeature::kCSSValuePrefixedMaxContent);
        break;
      case CSSValueWebkitFillAvailable:
        context.Count(WebFeature::kCSSValuePrefixedFillAvailable);
        break;
      case CSSValueWebkitFitContent:
        context.Count(WebFeature::kCSSValuePrefixedFitContent);
        break;
      default:
        break;
    }
    return true;
  }
  return false;
}

}  // namespace

CSSValue* CSSPropertyLengthUtils::ConsumeMaxWidthOrHeight(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPropertyParserHelpers::UnitlessQuirk unitless) {
  if (range.Peek().Id() == CSSValueNone ||
      ValidWidthOrHeightKeyword(range.Peek().Id(), context))
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative, unitless);
}

CSSValue* CSSPropertyLengthUtils::ConsumeWidthOrHeight(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPropertyParserHelpers::UnitlessQuirk unitless) {
  if (range.Peek().Id() == CSSValueAuto ||
      ValidWidthOrHeightKeyword(range.Peek().Id(), context))
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative, unitless);
}

}  // namespace blink
