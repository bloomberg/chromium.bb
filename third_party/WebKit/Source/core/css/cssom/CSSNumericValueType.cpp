// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSNumericValueType.h"

#include <algorithm>

namespace blink {

namespace {

CSSNumericValueType::BaseType UnitTypeToBaseType(
    CSSPrimitiveValue::UnitType unit) {
  using UnitType = CSSPrimitiveValue::UnitType;
  using BaseType = CSSNumericValueType::BaseType;

  DCHECK_NE(unit, UnitType::kNumber);
  switch (unit) {
    case UnitType::kEms:
    case UnitType::kExs:
    case UnitType::kPixels:
    case UnitType::kCentimeters:
    case UnitType::kMillimeters:
    case UnitType::kQuarterMillimeters:
    case UnitType::kInches:
    case UnitType::kPoints:
    case UnitType::kPicas:
    case UnitType::kUserUnits:
    case UnitType::kViewportWidth:
    case UnitType::kViewportHeight:
    case UnitType::kViewportMin:
    case UnitType::kViewportMax:
    case UnitType::kRems:
    case UnitType::kChs:
      return BaseType::kLength;
    case UnitType::kMilliseconds:
    case UnitType::kSeconds:
      return BaseType::kTime;
    case UnitType::kDegrees:
    case UnitType::kRadians:
    case UnitType::kGradians:
    case UnitType::kTurns:
      return BaseType::kAngle;
    case UnitType::kHertz:
    case UnitType::kKilohertz:
      return BaseType::kFrequency;
    case UnitType::kDotsPerPixel:
    case UnitType::kDotsPerInch:
    case UnitType::kDotsPerCentimeter:
      return BaseType::kResolution;
    case UnitType::kFraction:
      return BaseType::kFlex;
    case UnitType::kPercentage:
      return BaseType::kPercent;
    default:
      NOTREACHED();
      return BaseType::kLength;
  }
}

}  // namespace

CSSNumericValueType::CSSNumericValueType(CSSPrimitiveValue::UnitType unit) {
  if (unit != CSSPrimitiveValue::UnitType::kNumber)
    SetEntry(UnitTypeToBaseType(unit), 1);
}

/* static */
CSSNumericValueType CSSNumericValueType::NegateEntries(
    CSSNumericValueType type) {
  std::for_each(type.entries_.begin(), type.entries_.end(),
                [](int& v) { v *= -1; });
  return type;
}

}  // namespace blink
