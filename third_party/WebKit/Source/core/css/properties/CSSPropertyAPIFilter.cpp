// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIFilter.h"

#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSURIValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyBoxShadowUtils.h"
#include "core/frame/UseCounter.h"

namespace blink {
class CSSParserLocalContext;

namespace {

static CSSFunctionValue* ConsumeFilterFunction(
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  CSSValueID filter_type = range.Peek().FunctionId();
  if (filter_type < CSSValueInvert || filter_type > CSSValueDropShadow)
    return nullptr;
  CSSParserTokenRange args = CSSPropertyParserHelpers::ConsumeFunction(range);
  CSSFunctionValue* filter_value = CSSFunctionValue::Create(filter_type);
  CSSValue* parsed_value = nullptr;

  if (filter_type == CSSValueDropShadow) {
    parsed_value = CSSPropertyBoxShadowUtils::ParseSingleShadow(
        args, context.Mode(), AllowInsetAndSpread::kForbid);
  } else {
    if (args.AtEnd()) {
      context.Count(WebFeature::kCSSFilterFunctionNoArguments);
      return filter_value;
    }
    if (filter_type == CSSValueBrightness) {
      // FIXME (crbug.com/397061): Support calc expressions like calc(10% + 0.5)
      parsed_value =
          CSSPropertyParserHelpers::ConsumePercent(args, kValueRangeAll);
      if (!parsed_value) {
        parsed_value =
            CSSPropertyParserHelpers::ConsumeNumber(args, kValueRangeAll);
      }
    } else if (filter_type == CSSValueHueRotate) {
      parsed_value = CSSPropertyParserHelpers::ConsumeAngle(
          args, &context, WebFeature::kUnitlessZeroAngleFilter);
    } else if (filter_type == CSSValueBlur) {
      parsed_value = CSSPropertyParserHelpers::ConsumeLength(
          args, kHTMLStandardMode, kValueRangeNonNegative);
    } else {
      // FIXME (crbug.com/397061): Support calc expressions like calc(10% + 0.5)
      parsed_value = CSSPropertyParserHelpers::ConsumePercent(
          args, kValueRangeNonNegative);
      if (!parsed_value) {
        parsed_value = CSSPropertyParserHelpers::ConsumeNumber(
            args, kValueRangeNonNegative);
      }
      if (parsed_value && filter_type != CSSValueSaturate &&
          filter_type != CSSValueContrast) {
        bool is_percentage = ToCSSPrimitiveValue(parsed_value)->IsPercentage();
        double max_allowed = is_percentage ? 100.0 : 1.0;
        if (ToCSSPrimitiveValue(parsed_value)->GetDoubleValue() > max_allowed) {
          parsed_value = CSSPrimitiveValue::Create(
              max_allowed, is_percentage
                               ? CSSPrimitiveValue::UnitType::kPercentage
                               : CSSPrimitiveValue::UnitType::kNumber);
        }
      }
    }
  }
  if (!parsed_value || !args.AtEnd())
    return nullptr;
  filter_value->Append(*parsed_value);
  return filter_value;
}

}  // namespace

const CSSValue* CSSPropertyAPIFilter::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  do {
    CSSValue* filter_value =
        CSSPropertyParserHelpers::ConsumeUrl(range, &context);
    if (!filter_value) {
      filter_value = ConsumeFilterFunction(range, context);
      if (!filter_value)
        return nullptr;
    }
    list->Append(*filter_value);
  } while (!range.AtEnd());
  return list;
}

}  // namespace blink
