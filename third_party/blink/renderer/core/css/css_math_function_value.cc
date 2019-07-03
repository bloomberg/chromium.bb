// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_math_function_value.h"

#include "third_party/blink/renderer/core/css/css_calculation_value.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsCSSMathFunctionValue : CSSPrimitiveValue {
  Member<void*> calc;
};
ASSERT_SIZE(CSSMathFunctionValue, SameSizeAsCSSMathFunctionValue);

void CSSMathFunctionValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(calc_);
  CSSPrimitiveValue::TraceAfterDispatch(visitor);
}

CSSMathFunctionValue::CSSMathFunctionValue(CSSCalcValue* calc)
    : CSSPrimitiveValue(UnitType::kCalc, kMathFunctionClass), calc_(calc) {}

// static
CSSMathFunctionValue* CSSMathFunctionValue::Create(CSSCalcValue* calc) {
  return MakeGarbageCollected<CSSMathFunctionValue>(calc);
}

// static
CSSMathFunctionValue* CSSMathFunctionValue::Create(const Length& length,
                                                   float zoom) {
  const CalculationValue& calc = length.GetCalculationValue();
  return Create(CSSCalcValue::Create(
      CSSCalcValue::CreateExpressionNode(calc.Pixels() / zoom, calc.Percent()),
      calc.GetValueRange()));
}

CSSPrimitiveValue::UnitType CSSMathFunctionValue::TypeWithMathFunctionResolved()
    const {
  switch (calc_->Category()) {
    case kCalcAngle:
      return UnitType::kDegrees;
    case kCalcFrequency:
      return UnitType::kHertz;
    case kCalcNumber:
      return UnitType::kNumber;
    case kCalcPercent:
      return UnitType::kPercentage;
    case kCalcLength:
      return UnitType::kPixels;
    case kCalcPercentNumber:
      return UnitType::kCalcPercentageWithNumber;
    case kCalcPercentLength:
      return UnitType::kCalcPercentageWithLength;
    case kCalcLengthNumber:
      return UnitType::kCalcLengthWithNumber;
    case kCalcPercentLengthNumber:
      return UnitType::kCalcPercentageWithLengthAndNumber;
    case kCalcTime:
      return UnitType::kMilliseconds;
    case kCalcOther:
      return UnitType::kUnknown;
  }
  return UnitType::kUnknown;
}

double CSSMathFunctionValue::DoubleValue() const {
  // TODO(crbug.com/979895): Make sure this function is called only when
  // applicable.
  return calc_->DoubleValue();
}

double CSSMathFunctionValue::ComputeSeconds() const {
  DCHECK_EQ(kCalcTime, calc_->Category());
  UnitType current_type = calc_->ExpressionNode()->TypeWithCalcResolved();
  if (current_type == UnitType::kSeconds)
    return GetDoubleValue();
  if (current_type == UnitType::kMilliseconds)
    return GetDoubleValue() / 1000;
  NOTREACHED();
  return 0;
}

double CSSMathFunctionValue::ComputeDegrees() const {
  DCHECK_EQ(kCalcAngle, calc_->Category());
  UnitType current_type = calc_->ExpressionNode()->TypeWithCalcResolved();
  switch (current_type) {
    case UnitType::kDegrees:
      return GetDoubleValue();
    case UnitType::kRadians:
      return rad2deg(GetDoubleValue());
    case UnitType::kGradians:
      return grad2deg(GetDoubleValue());
    case UnitType::kTurns:
      return turn2deg(GetDoubleValue());
    default:
      NOTREACHED();
      return 0;
  }
}

double CSSMathFunctionValue::ComputeLengthPx(
    const CSSToLengthConversionData& conversion_data) const {
  return calc_->ComputeLengthPx(conversion_data);
}

void CSSMathFunctionValue::AccumulateLengthArray(CSSLengthArray& length_array,
                                                 double multiplier) const {
  calc_->AccumulateLengthArray(length_array, multiplier);
}

Length CSSMathFunctionValue::ConvertToLength(
    const CSSToLengthConversionData& conversion_data) const {
  return Length(calc_->ToCalcValue(conversion_data));
}

String CSSMathFunctionValue::CustomCSSText() const {
  return calc_->CustomCSSText();
}

bool CSSMathFunctionValue::Equals(const CSSMathFunctionValue& other) const {
  return calc_ && other.calc_ && calc_->Equals(*other.calc_);
}

}  // namespace blink
