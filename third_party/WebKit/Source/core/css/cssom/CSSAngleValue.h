// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSAngleValue_h
#define CSSAngleValue_h

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSStyleValue.h"

namespace blink {

class CORE_EXPORT CSSAngleValue final : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSAngleValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSAngleValue* Create(double value, const String& unit);
  static CSSAngleValue* Create(double value, CSSPrimitiveValue::UnitType);
  static CSSAngleValue* FromCSSValue(const CSSPrimitiveValue&);

  StyleValueType GetType() const override { return kAngleType; }

  double degrees() const;
  double radians() const;
  double gradians() const;
  double turns() const;

  double Value() const { return value_; }
  CSSPrimitiveValue::UnitType Unit() const { return unit_; }
  CSSValue* ToCSSValue() const override;

 private:
  CSSAngleValue(double value, CSSPrimitiveValue::UnitType unit)
      : value_(value), unit_(unit) {}

  double value_;
  CSSPrimitiveValue::UnitType unit_;
};
}

#endif  // CSSAngleValue_h
