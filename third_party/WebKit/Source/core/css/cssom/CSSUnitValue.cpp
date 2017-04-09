// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSUnitValue.h"

#include "bindings/core/v8/ExceptionState.h"

namespace blink {

namespace {

bool IsValidUnit(CSSPrimitiveValue::UnitType unit) {
  // UserUnits returns true for CSSPrimitiveValue::isLength below.
  if (unit == CSSPrimitiveValue::UnitType::kUserUnits)
    return false;
  if (unit == CSSPrimitiveValue::UnitType::kNumber ||
      unit == CSSPrimitiveValue::UnitType::kPercentage ||
      CSSPrimitiveValue::IsLength(unit) || CSSPrimitiveValue::IsAngle(unit) ||
      CSSPrimitiveValue::IsTime(unit) || CSSPrimitiveValue::IsFrequency(unit) ||
      CSSPrimitiveValue::IsResolution(unit) || CSSPrimitiveValue::IsFlex(unit))
    return true;
  return false;
}

}  // namespace

CSSPrimitiveValue::UnitType CSSUnitValue::UnitFromName(const String& name) {
  if (EqualIgnoringASCIICase(name, "number"))
    return CSSPrimitiveValue::UnitType::kNumber;
  if (EqualIgnoringASCIICase(name, "percent") || name == "%")
    return CSSPrimitiveValue::UnitType::kPercentage;
  return CSSPrimitiveValue::StringToUnitType(name);
}

CSSUnitValue* CSSUnitValue::Create(double value,
                                   const String& unit_name,
                                   ExceptionState& exception_state) {
  CSSPrimitiveValue::UnitType unit = CSSUnitValue::UnitFromName(unit_name);
  if (!IsValidUnit(unit)) {
    exception_state.ThrowTypeError("Invalid unit: " + unit_name);
    return nullptr;
  }
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
  CSSPrimitiveValue::UnitType unit = CSSUnitValue::UnitFromName(unit_name);
  if (!IsValidUnit(unit))
    exception_state.ThrowTypeError("Invalid unit: " + unit_name);

  unit_ = unit;
}

String CSSUnitValue::unit() const {
  if (unit_ == CSSPrimitiveValue::UnitType::kNumber)
    return "number";
  if (unit_ == CSSPrimitiveValue::UnitType::kPercentage)
    return "percent";
  return CSSPrimitiveValue::UnitTypeToString(unit_);
}

String CSSUnitValue::cssType() const {
  if (unit_ == CSSPrimitiveValue::UnitType::kNumber)
    return "number";
  if (unit_ == CSSPrimitiveValue::UnitType::kPercentage)
    return "percent";
  if (CSSPrimitiveValue::IsLength(unit_))
    return "length";
  if (CSSPrimitiveValue::IsAngle(unit_))
    return "angle";
  if (CSSPrimitiveValue::IsTime(unit_))
    return "time";
  if (CSSPrimitiveValue::IsFrequency(unit_))
    return "frequency";
  if (CSSPrimitiveValue::IsResolution(unit_))
    return "resolution";
  if (CSSPrimitiveValue::IsFlex(unit_))
    return "flex";
  return "";
}

const CSSValue* CSSUnitValue::ToCSSValue() const {
  return CSSPrimitiveValue::Create(value_, unit_);
}

}  // namespace blink
