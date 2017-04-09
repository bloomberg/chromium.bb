// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLengthValue_h
#define CSSLengthValue_h

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSStyleValue.h"

namespace blink {

class CSSCalcDictionary;
class ExceptionState;

class CORE_EXPORT CSSLengthValue : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSLengthValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const int kNumSupportedUnits = 15;

  static CSSLengthValue* from(const String& css_text, ExceptionState&);
  static CSSLengthValue* from(double value,
                              const String& type_str,
                              ExceptionState&);
  static CSSLengthValue* from(const CSSCalcDictionary&, ExceptionState&);

  static bool IsSupportedLengthUnit(CSSPrimitiveValue::UnitType unit) {
    return (CSSPrimitiveValue::IsLength(unit) ||
            unit == CSSPrimitiveValue::UnitType::kPercentage) &&
           unit != CSSPrimitiveValue::UnitType::kQuirkyEms &&
           unit != CSSPrimitiveValue::UnitType::kUserUnits;
  }
  static CSSPrimitiveValue::UnitType UnitFromName(const String& name);
  static CSSLengthValue* FromCSSValue(const CSSPrimitiveValue&);

  CSSLengthValue* add(const CSSLengthValue* other);
  CSSLengthValue* subtract(const CSSLengthValue* other);
  CSSLengthValue* multiply(double);
  CSSLengthValue* divide(double, ExceptionState&);

  virtual bool ContainsPercent() const = 0;

 protected:
  CSSLengthValue() {}

  virtual CSSLengthValue* AddInternal(const CSSLengthValue* other);
  virtual CSSLengthValue* SubtractInternal(const CSSLengthValue* other);
  virtual CSSLengthValue* MultiplyInternal(double);
  virtual CSSLengthValue* DivideInternal(double);
};

DEFINE_TYPE_CASTS(CSSLengthValue,
                  CSSStyleValue,
                  value,
                  (value->GetType() == CSSStyleValue::kSimpleLengthType ||
                   value->GetType() == CSSStyleValue::kCalcLengthType),
                  (value.GetType() == CSSStyleValue::kSimpleLengthType ||
                   value.GetType() == CSSStyleValue::kCalcLengthType));

}  // namespace blink

#endif
