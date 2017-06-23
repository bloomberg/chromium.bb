// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyTransformUtils.h"

#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/Length.h"

namespace blink {

namespace {

bool ConsumeNumbers(CSSParserTokenRange& args,
                    CSSFunctionValue*& transform_value,
                    unsigned number_of_arguments) {
  do {
    CSSValue* parsed_value =
        CSSPropertyParserHelpers::ConsumeNumber(args, kValueRangeAll);
    if (!parsed_value)
      return false;
    transform_value->Append(*parsed_value);
    if (--number_of_arguments &&
        !CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)) {
      return false;
    }
  } while (number_of_arguments);
  return true;
}

bool ConsumePerspective(CSSParserTokenRange& args,
                        const CSSParserContext* context,
                        CSSFunctionValue*& transform_value,
                        bool use_legacy_parsing) {
  CSSPrimitiveValue* parsed_value = CSSPropertyParserHelpers::ConsumeLength(
      args, context->Mode(), kValueRangeNonNegative);
  if (!parsed_value && use_legacy_parsing) {
    double perspective;
    if (!CSSPropertyParserHelpers::ConsumeNumberRaw(args, perspective) ||
        perspective < 0) {
      return false;
    }
    context->Count(WebFeature::kUnitlessPerspectiveInTransformProperty);
    parsed_value = CSSPrimitiveValue::Create(
        perspective, CSSPrimitiveValue::UnitType::kPixels);
  }
  if (!parsed_value)
    return false;
  transform_value->Append(*parsed_value);
  return true;
}

bool ConsumeTranslate3d(CSSParserTokenRange& args,
                        CSSParserMode css_parser_mode,
                        CSSFunctionValue*& transform_value) {
  unsigned number_of_arguments = 2;
  CSSValue* parsed_value = nullptr;
  do {
    parsed_value = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
        args, css_parser_mode, kValueRangeAll);
    if (!parsed_value)
      return false;
    transform_value->Append(*parsed_value);
    if (!CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args))
      return false;
  } while (--number_of_arguments);
  parsed_value = CSSPropertyParserHelpers::ConsumeLength(args, css_parser_mode,
                                                         kValueRangeAll);
  if (!parsed_value)
    return false;
  transform_value->Append(*parsed_value);
  return true;
}

CSSValue* ConsumeTransformValue(CSSParserTokenRange& range,
                                const CSSParserContext* context,
                                bool use_legacy_parsing) {
  CSSValueID function_id = range.Peek().FunctionId();
  if (function_id == CSSValueInvalid)
    return nullptr;
  CSSParserTokenRange args = CSSPropertyParserHelpers::ConsumeFunction(range);
  if (args.AtEnd())
    return nullptr;
  CSSFunctionValue* transform_value = CSSFunctionValue::Create(function_id);
  CSSValue* parsed_value = nullptr;
  switch (function_id) {
    case CSSValueRotate:
    case CSSValueRotateX:
    case CSSValueRotateY:
    case CSSValueRotateZ:
    case CSSValueSkewX:
    case CSSValueSkewY:
    case CSSValueSkew:
      parsed_value = CSSPropertyParserHelpers::ConsumeAngle(
          args, *context, WebFeature::kUnitlessZeroAngleTransform);
      if (!parsed_value)
        return nullptr;
      if (function_id == CSSValueSkew &&
          CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)) {
        transform_value->Append(*parsed_value);
        parsed_value = CSSPropertyParserHelpers::ConsumeAngle(
            args, *context, WebFeature::kUnitlessZeroAngleTransform);
        if (!parsed_value)
          return nullptr;
      }
      break;
    case CSSValueScaleX:
    case CSSValueScaleY:
    case CSSValueScaleZ:
    case CSSValueScale:
      parsed_value =
          CSSPropertyParserHelpers::ConsumeNumber(args, kValueRangeAll);
      if (!parsed_value)
        return nullptr;
      if (function_id == CSSValueScale &&
          CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)) {
        transform_value->Append(*parsed_value);
        parsed_value =
            CSSPropertyParserHelpers::ConsumeNumber(args, kValueRangeAll);
        if (!parsed_value)
          return nullptr;
      }
      break;
    case CSSValuePerspective:
      if (!ConsumePerspective(args, context, transform_value,
                              use_legacy_parsing)) {
        return nullptr;
      }
      break;
    case CSSValueTranslateX:
    case CSSValueTranslateY:
    case CSSValueTranslate:
      parsed_value = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
          args, context->Mode(), kValueRangeAll);
      if (!parsed_value)
        return nullptr;
      if (function_id == CSSValueTranslate &&
          CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)) {
        transform_value->Append(*parsed_value);
        parsed_value = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
            args, context->Mode(), kValueRangeAll);
        if (!parsed_value)
          return nullptr;
      }
      break;
    case CSSValueTranslateZ:
      parsed_value = CSSPropertyParserHelpers::ConsumeLength(
          args, context->Mode(), kValueRangeAll);
      break;
    case CSSValueMatrix:
    case CSSValueMatrix3d:
      if (!ConsumeNumbers(args, transform_value,
                          (function_id == CSSValueMatrix3d) ? 16 : 6)) {
        return nullptr;
      }
      break;
    case CSSValueScale3d:
      if (!ConsumeNumbers(args, transform_value, 3))
        return nullptr;
      break;
    case CSSValueRotate3d:
      if (!ConsumeNumbers(args, transform_value, 3) ||
          !CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)) {
        return nullptr;
      }
      parsed_value = CSSPropertyParserHelpers::ConsumeAngle(
          args, *context, WebFeature::kUnitlessZeroAngleTransform);
      if (!parsed_value)
        return nullptr;
      break;
    case CSSValueTranslate3d:
      if (!ConsumeTranslate3d(args, context->Mode(), transform_value))
        return nullptr;
      break;
    default:
      return nullptr;
  }
  if (parsed_value)
    transform_value->Append(*parsed_value);
  if (!args.AtEnd())
    return nullptr;
  return transform_value;
}

}  // namespace

CSSValue* CSSPropertyTransformUtils::ConsumeTransformList(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  do {
    CSSValue* parsed_transform_value =
        ConsumeTransformValue(range, &context, local_context.UseAliasParsing());
    if (!parsed_transform_value)
      return nullptr;
    list->Append(*parsed_transform_value);
  } while (!range.AtEnd());

  return list;
}

}  // namespace blink
