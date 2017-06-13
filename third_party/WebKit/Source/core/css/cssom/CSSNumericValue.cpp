// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSNumericValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSUnitValue.h"

namespace blink {

bool CSSNumericValue::IsValidUnit(CSSPrimitiveValue::UnitType unit) {
  // UserUnits returns true for CSSPrimitiveValue::IsLength below.
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

CSSPrimitiveValue::UnitType CSSNumericValue::UnitFromName(const String& name) {
  if (EqualIgnoringASCIICase(name, "number"))
    return CSSPrimitiveValue::UnitType::kNumber;
  if (EqualIgnoringASCIICase(name, "percent") || name == "%")
    return CSSPrimitiveValue::UnitType::kPercentage;
  return CSSPrimitiveValue::StringToUnitType(name);
}

CSSNumericValue* CSSNumericValue::parse(const String& css_text,
                                        ExceptionState&) {
  // TODO(meade): Implement
  return nullptr;
}

CSSNumericValue* CSSNumericValue::FromCSSValue(const CSSPrimitiveValue& value) {
  if (value.IsCalculated()) {
    // TODO(meade): Implement this case.
    return nullptr;
  }
  return CSSUnitValue::FromCSSValue(value);
}

CSSNumericValue* CSSNumericValue::to(const String& unit_string,
                                     ExceptionState& exception_state) {
  CSSPrimitiveValue::UnitType unit = UnitFromName(unit_string);
  if (!IsValidUnit(unit)) {
    exception_state.ThrowDOMException(kSyntaxError,
                                      "Invalid unit for conversion");
    return nullptr;
  }
  if (IsCalculated()) {
    exception_state.ThrowTypeError(
        "Conversion of CSSCalcValue is not supported yet");
    return nullptr;
  }
  CSSUnitValue* result = ToCSSUnitValue(this)->to(unit);
  if (!result) {
    exception_state.ThrowTypeError("Incompatible units for conversion");
    return nullptr;
  }
  return result;
}

}  // namespace blink
