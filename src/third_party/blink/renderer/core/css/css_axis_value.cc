// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_axis_value.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSAxisValue::CSSAxisValue(CSSValueID axis_name)
    : CSSValueList(kAxisClass, kSpaceSeparator), axis_name_(axis_name) {
  double x = 0;
  double y = 0;
  double z = 0;
  switch (axis_name) {
    case CSSValueX:
      x = 1;
      break;

    case CSSValueY:
      y = 1;
      break;

    case CSSValueZ:
      z = 1;
      break;

    default:
      NOTREACHED();
  }
  Append(*CSSPrimitiveValue::Create(x, CSSPrimitiveValue::UnitType::kNumber));
  Append(*CSSPrimitiveValue::Create(y, CSSPrimitiveValue::UnitType::kNumber));
  Append(*CSSPrimitiveValue::Create(z, CSSPrimitiveValue::UnitType::kNumber));
}

CSSAxisValue::CSSAxisValue(double x, double y, double z)
    : CSSValueList(kAxisClass, kSpaceSeparator), axis_name_(CSSValueInvalid) {
  // Normalize axis that are parallel to x, y or z axis.
  if (x > 0 && y == 0 && z == 0) {
    x = 1;
    axis_name_ = CSSValueX;
  } else if (x == 0 && y > 0 && z == 0) {
    y = 1;
    axis_name_ = CSSValueY;
  } else if (x == 0 && y == 0 && z > 0) {
    z = 1;
    axis_name_ = CSSValueZ;
  }
  Append(*CSSPrimitiveValue::Create(x, CSSPrimitiveValue::UnitType::kNumber));
  Append(*CSSPrimitiveValue::Create(y, CSSPrimitiveValue::UnitType::kNumber));
  Append(*CSSPrimitiveValue::Create(z, CSSPrimitiveValue::UnitType::kNumber));
}

String CSSAxisValue::CustomCSSText() const {
  StringBuilder result;
  if (axis_name_ != CSSValueInvalid) {
    result.Append(AtomicString(getValueName(axis_name_)));
  } else {
    result.Append(CSSValueList::CustomCSSText());
  }
  return result.ToString();
}

double CSSAxisValue::X() const {
  return ToCSSPrimitiveValue(Item(0)).GetDoubleValue();
}

double CSSAxisValue::Y() const {
  return ToCSSPrimitiveValue(Item(1)).GetDoubleValue();
}

double CSSAxisValue::Z() const {
  return ToCSSPrimitiveValue(Item(2)).GetDoubleValue();
}

}  // namespace blink
