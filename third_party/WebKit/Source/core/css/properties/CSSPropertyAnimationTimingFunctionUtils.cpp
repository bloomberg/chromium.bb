// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAnimationTimingFunctionUtils.h"

#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

namespace {

CSSValue* ConsumeSteps(CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueSteps);
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args =
      CSSPropertyParserHelpers::ConsumeFunction(range_copy);

  CSSPrimitiveValue* steps =
      CSSPropertyParserHelpers::ConsumePositiveInteger(args);
  if (!steps)
    return nullptr;

  StepsTimingFunction::StepPosition position =
      StepsTimingFunction::StepPosition::END;
  if (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)) {
    switch (args.ConsumeIncludingWhitespace().Id()) {
      case CSSValueMiddle:
        if (!RuntimeEnabledFeatures::WebAnimationsAPIEnabled())
          return nullptr;
        position = StepsTimingFunction::StepPosition::MIDDLE;
        break;
      case CSSValueStart:
        position = StepsTimingFunction::StepPosition::START;
        break;
      case CSSValueEnd:
        position = StepsTimingFunction::StepPosition::END;
        break;
      default:
        return nullptr;
    }
  }

  if (!args.AtEnd())
    return nullptr;

  range = range_copy;
  return CSSStepsTimingFunctionValue::Create(steps->GetIntValue(), position);
}

CSSValue* ConsumeFrames(CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueFrames);
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args =
      CSSPropertyParserHelpers::ConsumeFunction(range_copy);

  CSSPrimitiveValue* frames =
      CSSPropertyParserHelpers::ConsumePositiveInteger(args);
  if (!frames)
    return nullptr;

  int frames_int = frames->GetIntValue();
  if (frames_int <= 1)
    return nullptr;

  if (!args.AtEnd())
    return nullptr;

  range = range_copy;
  return CSSFramesTimingFunctionValue::Create(frames_int);
}

CSSValue* ConsumeCubicBezier(CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().FunctionId(), CSSValueCubicBezier);
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args =
      CSSPropertyParserHelpers::ConsumeFunction(range_copy);

  double x1, y1, x2, y2;
  if (CSSPropertyParserHelpers::ConsumeNumberRaw(args, x1) && x1 >= 0 &&
      x1 <= 1 &&
      CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args) &&
      CSSPropertyParserHelpers::ConsumeNumberRaw(args, y1) &&
      CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args) &&
      CSSPropertyParserHelpers::ConsumeNumberRaw(args, x2) && x2 >= 0 &&
      x2 <= 1 &&
      CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args) &&
      CSSPropertyParserHelpers::ConsumeNumberRaw(args, y2) && args.AtEnd()) {
    range = range_copy;
    return CSSCubicBezierTimingFunctionValue::Create(x1, y1, x2, y2);
  }

  return nullptr;
}

}  // namespace

CSSValue*
CSSPropertyAnimationTimingFunctionUtils::ConsumeAnimationTimingFunction(
    CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueEase || id == CSSValueLinear || id == CSSValueEaseIn ||
      id == CSSValueEaseOut || id == CSSValueEaseInOut ||
      id == CSSValueStepStart || id == CSSValueStepEnd ||
      id == CSSValueStepMiddle)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValueID function = range.Peek().FunctionId();
  if (function == CSSValueSteps)
    return ConsumeSteps(range);
  if (RuntimeEnabledFeatures::FramesTimingFunctionEnabled() &&
      function == CSSValueFrames) {
    return ConsumeFrames(range);
  }
  if (function == CSSValueCubicBezier)
    return ConsumeCubicBezier(range);
  return nullptr;
}

}  // namespace blink
