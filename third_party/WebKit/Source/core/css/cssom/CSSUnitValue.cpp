// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSUnitValue.h"

#include "bindings/core/v8/ExceptionState.h"

namespace blink {

namespace {

bool isValidUnit(CSSPrimitiveValue::UnitType unit) {
  // UserUnits returns true for CSSPrimitiveValue::isLength below.
  if (unit == CSSPrimitiveValue::UnitType::UserUnits)
    return false;
  if (unit == CSSPrimitiveValue::UnitType::Number ||
      unit == CSSPrimitiveValue::UnitType::Percentage ||
      CSSPrimitiveValue::isLength(unit) || CSSPrimitiveValue::isAngle(unit) ||
      CSSPrimitiveValue::isTime(unit) || CSSPrimitiveValue::isFrequency(unit) ||
      CSSPrimitiveValue::isResolution(unit) || CSSPrimitiveValue::isFlex(unit))
    return true;
  return false;
}

}  // namespace

CSSPrimitiveValue::UnitType CSSUnitValue::unitFromName(const String& name) {
  if (equalIgnoringASCIICase(name, "number"))
    return CSSPrimitiveValue::UnitType::Number;
  if (equalIgnoringASCIICase(name, "percent") || name == "%")
    return CSSPrimitiveValue::UnitType::Percentage;
  return CSSPrimitiveValue::stringToUnitType(name);
}

CSSUnitValue* CSSUnitValue::create(double value,
                                   const String& unitName,
                                   ExceptionState& exceptionState) {
  CSSPrimitiveValue::UnitType unit = CSSUnitValue::unitFromName(unitName);
  if (!isValidUnit(unit)) {
    exceptionState.throwTypeError("Invalid unit: " + unitName);
    return nullptr;
  }
  return new CSSUnitValue(value, unit);
}

CSSUnitValue* CSSUnitValue::fromCSSValue(
    const CSSPrimitiveValue& cssPrimitiveValue) {
  if (!isValidUnit(cssPrimitiveValue.typeWithCalcResolved()))
    return nullptr;
  return new CSSUnitValue(cssPrimitiveValue.getDoubleValue(),
                          cssPrimitiveValue.typeWithCalcResolved());
}

void CSSUnitValue::setUnit(const String& unitName,
                           ExceptionState& exceptionState) {
  CSSPrimitiveValue::UnitType unit = CSSUnitValue::unitFromName(unitName);
  if (!isValidUnit(unit))
    exceptionState.throwTypeError("Invalid unit: " + unitName);

  m_unit = unit;
}

String CSSUnitValue::unit() const {
  if (m_unit == CSSPrimitiveValue::UnitType::Number)
    return "number";
  if (m_unit == CSSPrimitiveValue::UnitType::Percentage)
    return "percent";
  return CSSPrimitiveValue::unitTypeToString(m_unit);
}

String CSSUnitValue::cssType() const {
  if (m_unit == CSSPrimitiveValue::UnitType::Number)
    return "number";
  if (m_unit == CSSPrimitiveValue::UnitType::Percentage)
    return "percent";
  if (CSSPrimitiveValue::isLength(m_unit))
    return "length";
  if (CSSPrimitiveValue::isAngle(m_unit))
    return "angle";
  if (CSSPrimitiveValue::isTime(m_unit))
    return "time";
  if (CSSPrimitiveValue::isFrequency(m_unit))
    return "frequency";
  if (CSSPrimitiveValue::isResolution(m_unit))
    return "resolution";
  if (CSSPrimitiveValue::isFlex(m_unit))
    return "flex";
  return "";
}

const CSSValue* CSSUnitValue::toCSSValue() const {
  return CSSPrimitiveValue::create(m_value, m_unit);
}

}  // namespace blink
