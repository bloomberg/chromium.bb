// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMathNegate_h
#define CSSMathNegate_h

#include "core/css/cssom/CSSMathValue.h"

namespace blink {

// Represents the negation of a CSSNumericValue.
// See CSSMathNegate.idl for more information about this class.
class CORE_EXPORT CSSMathNegate : public CSSMathValue {
  WTF_MAKE_NONCOPYABLE(CSSMathNegate);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
  static CSSMathNegate* Create(const CSSNumberish& arg) {
    return Create(CSSNumericValue::FromNumberish(arg));
  }
  // Blink-internal constructor
  static CSSMathNegate* Create(CSSNumericValue* value) {
    return new CSSMathNegate(
        value, CSSNumericValueType::NegateExponents(value->Type()));
  }

  String getOperator() const final { return "negate"; }

  void value(CSSNumberish& value) { value.SetCSSNumericValue(value_); }
  void setValue(const CSSNumberish& value) {
    value_ = CSSNumericValue::FromNumberish(value);
  }

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kNegateType; }

  void Trace(Visitor* visitor) override {
    visitor->Trace(value_);
    CSSMathValue::Trace(visitor);
  }

 private:
  CSSMathNegate(CSSNumericValue* value, const CSSNumericValueType& type)
      : CSSMathValue(type), value_(value) {}

  Member<CSSNumericValue> value_;
};

}  // namespace blink

#endif  // CSSMathNegate_h
