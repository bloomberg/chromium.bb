// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSUnitValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSResolutionUnits.h"
#include "core/css/cssom/CSSMathMax.h"
#include "core/css/cssom/CSSMathMin.h"
#include "core/css/cssom/CSSMathProduct.h"
#include "core/css/cssom/CSSMathSum.h"
#include "core/css/cssom/CSSNumericSumValue.h"
#include "platform/wtf/MathExtras.h"

namespace blink {

namespace {

CSSPrimitiveValue::UnitType ToCanonicalUnit(CSSPrimitiveValue::UnitType unit) {
  return CSSPrimitiveValue::CanonicalUnitTypeForCategory(
      CSSPrimitiveValue::UnitTypeToUnitCategory(unit));
}

}  // namespace

CSSUnitValue* CSSUnitValue::Create(double value,
                                   const String& unit_name,
                                   ExceptionState& exception_state) {
  CSSPrimitiveValue::UnitType unit = UnitFromName(unit_name);
  if (!IsValidUnit(unit)) {
    exception_state.ThrowTypeError("Invalid unit: " + unit_name);
    return nullptr;
  }
  return new CSSUnitValue(value, unit);
}

CSSUnitValue* CSSUnitValue::Create(double value,
                                   CSSPrimitiveValue::UnitType unit) {
  DCHECK(IsValidUnit(unit));
  return new CSSUnitValue(value, unit);
}

CSSUnitValue* CSSUnitValue::FromCSSValue(
    const CSSPrimitiveValue& css_primitive_value) {
  if (!IsValidUnit(css_primitive_value.TypeWithCalcResolved()))
    return nullptr;
  return new CSSUnitValue(css_primitive_value.GetDoubleValue(),
                          css_primitive_value.TypeWithCalcResolved());
}

void CSSUnitValue::setUnit(const String& unit_name,
                           ExceptionState& exception_state) {
  CSSPrimitiveValue::UnitType unit = UnitFromName(unit_name);
  if (!IsValidUnit(unit)) {
    exception_state.ThrowTypeError("Invalid unit: " + unit_name);
    return;
  }

  unit_ = unit;
}

String CSSUnitValue::unit() const {
  if (unit_ == CSSPrimitiveValue::UnitType::kNumber)
    return "number";
  if (unit_ == CSSPrimitiveValue::UnitType::kPercentage)
    return "percent";
  return CSSPrimitiveValue::UnitTypeToString(unit_);
}

String CSSUnitValue::type() const {
  return StyleValueTypeToString(GetType());
}

CSSStyleValue::StyleValueType CSSUnitValue::GetType() const {
  if (unit_ == CSSPrimitiveValue::UnitType::kNumber)
    return StyleValueType::kNumberType;
  if (unit_ == CSSPrimitiveValue::UnitType::kPercentage)
    return StyleValueType::kPercentType;
  if (CSSPrimitiveValue::IsLength(unit_))
    return StyleValueType::kLengthType;
  if (CSSPrimitiveValue::IsAngle(unit_))
    return StyleValueType::kAngleType;
  if (CSSPrimitiveValue::IsTime(unit_))
    return StyleValueType::kTimeType;
  if (CSSPrimitiveValue::IsFrequency(unit_))
    return StyleValueType::kFrequencyType;
  if (CSSPrimitiveValue::IsResolution(unit_))
    return StyleValueType::kResolutionType;
  if (CSSPrimitiveValue::IsFlex(unit_))
    return StyleValueType::kFlexType;
  NOTREACHED();
  return StyleValueType::kUnknownType;
}

const CSSValue* CSSUnitValue::ToCSSValue(SecureContextMode) const {
  return CSSPrimitiveValue::Create(value_, unit_);
}

CSSUnitValue* CSSUnitValue::ConvertTo(CSSPrimitiveValue::UnitType unit) const {
  if (unit_ == unit)
    return Create(value_, unit_);

  // TODO(meade): Implement other types: time, frequency and resolution.
  if (CSSPrimitiveValue::IsLength(unit_) && CSSPrimitiveValue::IsLength(unit)) {
    // Only fixed lengths can be converted.
    if (CSSPrimitiveValue::IsRelativeUnit(unit_) ||
        CSSPrimitiveValue::IsRelativeUnit(unit))
      return nullptr;
    return Create(ConvertFixedLength(unit), unit);
  }
  if (CSSPrimitiveValue::IsAngle(unit_) && CSSPrimitiveValue::IsAngle(unit))
    return Create(ConvertAngle(unit), unit);

  return nullptr;
}

WTF::Optional<CSSNumericSumValue> CSSUnitValue::SumValue() const {
  const auto canonical_unit = ToCanonicalUnit(unit_);
  if (canonical_unit == CSSPrimitiveValue::UnitType::kUnknown)
    return WTF::nullopt;

  CSSNumericSumValue sum;
  CSSNumericSumValue::UnitMap unit_map;
  if (unit_ != CSSPrimitiveValue::UnitType::kNumber)
    unit_map.insert(canonical_unit, 1);

  sum.terms.emplace_back(
      value_ * CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(unit_),
      std::move(unit_map));
  return sum;
}

double CSSUnitValue::ConvertFixedLength(
    CSSPrimitiveValue::UnitType unit) const {
  switch (unit_) {
    case CSSPrimitiveValue::UnitType::kPixels:
      switch (unit) {
        case CSSPrimitiveValue::UnitType::kCentimeters:
          return value_ / kCssPixelsPerCentimeter;
        case CSSPrimitiveValue::UnitType::kMillimeters:
          return value_ / kCssPixelsPerMillimeter;
        case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
          return value_ / kCssPixelsPerQuarterMillimeter;
        case CSSPrimitiveValue::UnitType::kInches:
          return value_ / kCssPixelsPerInch;
        case CSSPrimitiveValue::UnitType::kPoints:
          return value_ / kCssPixelsPerPoint;
        case CSSPrimitiveValue::UnitType::kPicas:
          return value_ / kCssPixelsPerPica;
        default:
          NOTREACHED();
          return 0;
      }
    case CSSPrimitiveValue::UnitType::kCentimeters:
      switch (unit) {
        case CSSPrimitiveValue::UnitType::kPixels:
          return value_ * kCssPixelsPerCentimeter;
        case CSSPrimitiveValue::UnitType::kMillimeters:
          return value_ * kMillimetersPerCentimeter;
        case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
          return value_ * kQuarterMillimetersPerCentimeter;
        case CSSPrimitiveValue::UnitType::kInches:
          return value_ / kCentimetersPerInch;
        case CSSPrimitiveValue::UnitType::kPoints:
          return value_ * (kPointsPerInch / kCentimetersPerInch);
        case CSSPrimitiveValue::UnitType::kPicas:
          return value_ * (kPicasPerInch / kCentimetersPerInch);
        default:
          NOTREACHED();
          return 0;
      }
    case CSSPrimitiveValue::UnitType::kMillimeters:
      switch (unit) {
        case CSSPrimitiveValue::UnitType::kPixels:
          return value_ * kCssPixelsPerMillimeter;
        case CSSPrimitiveValue::UnitType::kCentimeters:
          return value_ / kMillimetersPerCentimeter;
        case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
          return value_ *
                 (kQuarterMillimetersPerCentimeter / kMillimetersPerCentimeter);
        case CSSPrimitiveValue::UnitType::kInches:
          return value_ / (kMillimetersPerCentimeter * kCentimetersPerInch);
        case CSSPrimitiveValue::UnitType::kPoints:
          return value_ * (kPointsPerInch / kMillimetersPerInch);
        case CSSPrimitiveValue::UnitType::kPicas:
          return value_ * (kPicasPerInch / kMillimetersPerInch);
        default:
          NOTREACHED();
          return 0;
      }
    case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
      switch (unit) {
        case CSSPrimitiveValue::UnitType::kPixels:
          return value_ * kCssPixelsPerQuarterMillimeter;
        case CSSPrimitiveValue::UnitType::kCentimeters:
          return value_ / kQuarterMillimetersPerCentimeter;
        case CSSPrimitiveValue::UnitType::kMillimeters:
          return value_ /
                 (kQuarterMillimetersPerCentimeter / kMillimetersPerCentimeter);
        case CSSPrimitiveValue::UnitType::kInches:
          return value_ /
                 (kQuarterMillimetersPerCentimeter * kCentimetersPerInch);
        case CSSPrimitiveValue::UnitType::kPoints:
          return value_ * (kPointsPerInch / kQuarterMillimetersPerInch);
        case CSSPrimitiveValue::UnitType::kPicas:
          return value_ * (kPicasPerInch / kQuarterMillimetersPerInch);
        default:
          NOTREACHED();
          return 0;
      }
    case CSSPrimitiveValue::UnitType::kInches:
      switch (unit) {
        case CSSPrimitiveValue::UnitType::kPixels:
          return value_ * kCssPixelsPerInch;
        case CSSPrimitiveValue::UnitType::kMillimeters:
          return value_ * kCentimetersPerInch * kMillimetersPerCentimeter;
        case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
          return value_ * kCentimetersPerInch *
                 kQuarterMillimetersPerCentimeter;
        case CSSPrimitiveValue::UnitType::kCentimeters:
          return value_ * kCentimetersPerInch;
        case CSSPrimitiveValue::UnitType::kPoints:
          return value_ * kPointsPerInch;
        case CSSPrimitiveValue::UnitType::kPicas:
          return value_ * kPicasPerInch;
        default:
          NOTREACHED();
          return 0;
      }
    case CSSPrimitiveValue::UnitType::kPoints:
      switch (unit) {
        case CSSPrimitiveValue::UnitType::kPixels:
          return value_ * kCssPixelsPerPoint;
        case CSSPrimitiveValue::UnitType::kMillimeters:
          return value_ * (kMillimetersPerInch / kPointsPerInch);
        case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
          return value_ * (kQuarterMillimetersPerInch / kPointsPerInch);
        case CSSPrimitiveValue::UnitType::kCentimeters:
          return value_ * (kCentimetersPerInch / kPointsPerInch);
        case CSSPrimitiveValue::UnitType::kInches:
          return value_ / kPointsPerInch;
        case CSSPrimitiveValue::UnitType::kPicas:
          return value_ * (kPicasPerInch / kPointsPerInch);
        default:
          NOTREACHED();
          return 0;
      }
    case CSSPrimitiveValue::UnitType::kPicas:
      switch (unit) {
        case CSSPrimitiveValue::UnitType::kPixels:
          return value_ * kCssPixelsPerPica;
        case CSSPrimitiveValue::UnitType::kMillimeters:
          return value_ * (kMillimetersPerInch / kPicasPerInch);
        case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
          return value_ * (kQuarterMillimetersPerInch / kPicasPerInch);
        case CSSPrimitiveValue::UnitType::kCentimeters:
          return value_ * (kCentimetersPerInch / kPicasPerInch);
        case CSSPrimitiveValue::UnitType::kInches:
          return value_ / kPicasPerInch;
        case CSSPrimitiveValue::UnitType::kPoints:
          return value_ * (kPointsPerInch / kPicasPerInch);
        default:
          NOTREACHED();
          return 0;
      }
    default:
      NOTREACHED();
      return 0;
  }
}

double CSSUnitValue::ConvertAngle(CSSPrimitiveValue::UnitType unit) const {
  switch (unit_) {
    case CSSPrimitiveValue::UnitType::kDegrees:
      switch (unit) {
        case CSSPrimitiveValue::UnitType::kRadians:
          return deg2rad(value_);
        case CSSPrimitiveValue::UnitType::kGradians:
          return deg2grad(value_);
        case CSSPrimitiveValue::UnitType::kTurns:
          return deg2turn(value_);
        default:
          NOTREACHED();
          return 0;
      }
    case CSSPrimitiveValue::UnitType::kRadians:
      switch (unit) {
        case CSSPrimitiveValue::UnitType::kDegrees:
          return rad2deg(value_);
        case CSSPrimitiveValue::UnitType::kGradians:
          return rad2grad(value_);
        case CSSPrimitiveValue::UnitType::kTurns:
          return rad2turn(value_);
        default:
          NOTREACHED();
          return 0;
      }
    case CSSPrimitiveValue::UnitType::kGradians:
      switch (unit) {
        case CSSPrimitiveValue::UnitType::kDegrees:
          return grad2deg(value_);
        case CSSPrimitiveValue::UnitType::kRadians:
          return grad2rad(value_);
        case CSSPrimitiveValue::UnitType::kTurns:
          return grad2turn(value_);
        default:
          NOTREACHED();
          return 0;
      }
    case CSSPrimitiveValue::UnitType::kTurns:
      switch (unit) {
        case CSSPrimitiveValue::UnitType::kDegrees:
          return turn2deg(value_);
        case CSSPrimitiveValue::UnitType::kRadians:
          return turn2rad(value_);
        case CSSPrimitiveValue::UnitType::kGradians:
          return turn2grad(value_);
        default:
          NOTREACHED();
          return 0;
      }
    default:
      NOTREACHED();
      return 0;
  }
}

bool CSSUnitValue::Equals(const CSSNumericValue& other) const {
  if (!other.IsUnitValue())
    return false;

  const CSSUnitValue& other_unit_value = ToCSSUnitValue(other);
  return value_ == other_unit_value.value_ && unit_ == other_unit_value.unit_;
}

}  // namespace blink
