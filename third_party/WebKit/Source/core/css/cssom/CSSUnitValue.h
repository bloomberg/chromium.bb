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
  static CSSPrimitiveValue::UnitType UnitFromName(const String& name);
  static CSSUnitValue* Create(double value,
                              const String& unit,
                              ExceptionState&);
  static CSSUnitValue* FromCSSValue(const CSSPrimitiveValue&);

  void setValue(double new_value) { value_ = new_value; }
  double value() const { return value_; }
  void setUnit(const String& new_unit, ExceptionState&);
  String unit() const;

  String cssType() const;

  StyleValueType GetType() const override { return StyleValueType::kUnitType; }

  const CSSValue* ToCSSValue() const override;

 private:
  CSSUnitValue(double value, CSSPrimitiveValue::UnitType unit)
      : CSSNumericValue(), value_(value), unit_(unit) {}

  double value_;
  CSSPrimitiveValue::UnitType unit_;
};

}  // namespace blink

#endif  // CSSUnitValue_h
