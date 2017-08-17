// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyBackgroundUtils.h"

#include "core/CSSValueKeywords.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

void CSSPropertyBackgroundUtils::AddBackgroundValue(CSSValue*& list,
                                                    CSSValue* value) {
  if (list) {
    if (!list->IsBaseValueList()) {
      CSSValue* first_value = list;
      list = CSSValueList::CreateCommaSeparated();
      ToCSSValueList(list)->Append(*first_value);
    }
    ToCSSValueList(list)->Append(*value);
  } else {
    // To conserve memory we don't actually wrap a single value in a list.
    list = value;
  }
}

CSSValue* CSSPropertyBackgroundUtils::ConsumeBackgroundAttachment(
    CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::ConsumeIdent<CSSValueScroll, CSSValueFixed,
                                                CSSValueLocal>(range);
}

CSSValue* CSSPropertyBackgroundUtils::ConsumeBackgroundBlendMode(
    CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNormal || id == CSSValueOverlay ||
      (id >= CSSValueMultiply && id <= CSSValueLuminosity))
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return nullptr;
}

CSSValue* CSSPropertyBackgroundUtils::ConsumeBackgroundBox(
    CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::ConsumeIdent<
      CSSValueBorderBox, CSSValuePaddingBox, CSSValueContentBox>(range);
}

CSSValue* CSSPropertyBackgroundUtils::ConsumeBackgroundComposite(
    CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::ConsumeIdentRange(range, CSSValueClear,
                                                     CSSValuePlusLighter);
}

CSSValue* CSSPropertyBackgroundUtils::ConsumeMaskSourceType(
    CSSParserTokenRange& range) {
  DCHECK(RuntimeEnabledFeatures::CSSMaskSourceTypeEnabled());
  return CSSPropertyParserHelpers::ConsumeIdent<CSSValueAuto, CSSValueAlpha,
                                                CSSValueLuminance>(range);
}

CSSValue* CSSPropertyBackgroundUtils::ConsumeBackgroundSize(
    CSSParserTokenRange& range,
    CSSParserMode css_parser_mode,
    ParsingStyle parsing_style) {
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueContain, CSSValueCover>(
          range.Peek().Id())) {
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  }

  CSSValue* horizontal =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueAuto>(range);
  if (!horizontal) {
    horizontal = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
        range, css_parser_mode, kValueRangeAll,
        CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
  }

  CSSValue* vertical = nullptr;
  if (!range.AtEnd()) {
    if (range.Peek().Id() == CSSValueAuto) {  // `auto' is the default
      range.ConsumeIncludingWhitespace();
    } else {
      vertical = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
          range, css_parser_mode, kValueRangeAll,
          CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
    }
  } else if (parsing_style == ParsingStyle::kLegacy) {
    // Legacy syntax: "-webkit-background-size: 10px" is equivalent to
    // "background-size: 10px 10px".
    vertical = horizontal;
  }
  if (!vertical)
    return horizontal;
  return CSSValuePair::Create(horizontal, vertical,
                              CSSValuePair::kKeepIdenticalValues);
}

bool CSSPropertyBackgroundUtils::ConsumeBackgroundPosition(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    CSSPropertyParserHelpers::UnitlessQuirk unitless,
    CSSValue*& result_x,
    CSSValue*& result_y) {
  do {
    CSSValue* position_x = nullptr;
    CSSValue* position_y = nullptr;
    if (!CSSPropertyParserHelpers::ConsumePosition(
            range, context, unitless,
            WebFeature::kThreeValuedPositionBackground, position_x, position_y))
      return false;
    AddBackgroundValue(result_x, position_x);
    AddBackgroundValue(result_y, position_y);
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));
  return true;
}

}  // namespace blink
