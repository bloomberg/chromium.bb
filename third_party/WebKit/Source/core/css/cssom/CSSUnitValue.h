// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSUnitValue_h
#define CSSUnitValue_h

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSNumericValue.h"

namespace blink {

class CORE_EXPORT CSSUnitValue final : public CSSNumericValue {
  WTF_MAKE_NONCOPYABLE(CSSUnitValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSPrimitiveValue::UnitType unitFromName(const String& name);
  static CSSUnitValue* create(double value,
                              const String& unit,
                              ExceptionState&);
  static CSSUnitValue* fromCSSValue(const CSSPrimitiveValue&);

  void setValue(double newValue) { m_value = newValue; }
  double value() const { return m_value; }
  void setUnit(const String& newUnit, ExceptionState&);
  String unit() const;

  String cssType() const;

  StyleValueType type() const override { return StyleValueType::UnitType; }

  const CSSValue* toCSSValue() const override;

 private:
  CSSUnitValue(double value, CSSPrimitiveValue::UnitType unit)
      : CSSNumericValue(), m_value(value), m_unit(unit) {}

  double m_value;
  CSSPrimitiveValue::UnitType m_unit;
};

}  // namespace blink

#endif  // CSSUnitValue_h
