// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSimpleLength_h
#define CSSSimpleLength_h

#include "core/css/cssom/CSSLengthValue.h"

namespace blink {

class CSSPrimitiveValue;
class ExceptionState;

class CORE_EXPORT CSSSimpleLength final : public CSSLengthValue {
  WTF_MAKE_NONCOPYABLE(CSSSimpleLength);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSSimpleLength* Create(double, const String& type, ExceptionState&);
  static CSSSimpleLength* Create(double value,
                                 CSSPrimitiveValue::UnitType type) {
    return new CSSSimpleLength(value, type);
  }
  static CSSSimpleLength* FromCSSValue(const CSSPrimitiveValue&);

  bool ContainsPercent() const override;

  double value() const { return value_; }
  String unit() const;
  CSSPrimitiveValue::UnitType LengthUnit() const { return unit_; }

  StyleValueType GetType() const override {
    return StyleValueType::kSimpleLengthType;
  }

  CSSValue* ToCSSValue() const override;

 protected:
  virtual CSSLengthValue* AddInternal(const CSSLengthValue* other);
  virtual CSSLengthValue* SubtractInternal(const CSSLengthValue* other);
  virtual CSSLengthValue* MultiplyInternal(double);
  virtual CSSLengthValue* DivideInternal(double);

 private:
  CSSSimpleLength(double value, CSSPrimitiveValue::UnitType unit)
      : CSSLengthValue(), unit_(unit), value_(value) {}

  CSSPrimitiveValue::UnitType unit_;
  double value_;
};

#define DEFINE_SIMPLE_LENGTH_TYPE_CASTS(argumentType)                        \
  DEFINE_TYPE_CASTS(                                                         \
      CSSSimpleLength, argumentType, value,                                  \
      value->GetType() == CSSLengthValue::StyleValueType::kSimpleLengthType, \
      value.GetType() == CSSLengthValue::StyleValueType::kSimpleLengthType)

DEFINE_SIMPLE_LENGTH_TYPE_CASTS(CSSLengthValue);
DEFINE_SIMPLE_LENGTH_TYPE_CASTS(CSSStyleValue);

}  // namespace blink

#endif
