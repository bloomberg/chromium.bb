// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSUnitValue_h
#define CSSUnitValue_h

#include "base/macros.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSNumericValue.h"

namespace blink {

// Represents numeric values that can be expressed as a single number plus a
// unit (or a naked number or percentage).
// See CSSUnitValue.idl for more information about this class.
class CORE_EXPORT CSSUnitValue final : public CSSNumericValue {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
  static CSSUnitValue* Create(double value,
                              const String& unit,
                              ExceptionState&);
  // Blink-internal ways of creating CSSUnitValues.
  static CSSUnitValue* Create(
      double value,
      CSSPrimitiveValue::UnitType = CSSPrimitiveValue::UnitType::kNumber);
  static CSSUnitValue* FromCSSValue(const CSSPrimitiveValue&);

  // Setters and getters for attributes defined in the IDL.
  void setValue(double new_value) { value_ = new_value; }
  double value() const { return value_; }
  void setUnit(const String& new_unit, ExceptionState&);
  String unit() const;
  String type() const;

  // Internal methods.
  CSSPrimitiveValue::UnitType GetInternalUnit() const { return unit_; }
  CSSUnitValue* ConvertTo(CSSPrimitiveValue::UnitType) const;

  // From CSSNumericValue.
  bool IsUnitValue() const final { return true; }
  WTF::Optional<CSSNumericSumValue> SumValue() const final;

  bool Equals(const CSSNumericValue&) const final;

  // From CSSStyleValue.
  StyleValueType GetType() const final;
  bool ContainsPercent() const final {
    return unit_ == CSSPrimitiveValue::UnitType::kPercentage;
  }
  const CSSValue* ToCSSValue(SecureContextMode) const final;

 private:
  CSSUnitValue(double value, CSSPrimitiveValue::UnitType unit)
      : CSSNumericValue(CSSNumericValueType(unit)),
        value_(value),
        unit_(unit) {}

  double ConvertFixedLength(CSSPrimitiveValue::UnitType) const;
  double ConvertAngle(CSSPrimitiveValue::UnitType) const;

  // From CSSNumericValue
  CSSNumericValue* Negate() final {
    return CSSUnitValue::Create(-value_, unit_);
  }
  CSSNumericValue* Invert() final {
    return CSSUnitValue::Create(1.0 / value_, unit_);
  }

  double value_;
  CSSPrimitiveValue::UnitType unit_;
  DISALLOW_COPY_AND_ASSIGN(CSSUnitValue);
};

DEFINE_TYPE_CASTS(CSSUnitValue,
                  CSSNumericValue,
                  value,
                  value->IsUnitValue(),
                  value.IsUnitValue());

}  // namespace blink

#endif  // CSSUnitValue_h
