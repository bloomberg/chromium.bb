// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMathInvert_h
#define CSSMathInvert_h

#include "core/css/cssom/CSSMathValue.h"

namespace blink {

// Represents the inverse of a CSSNumericValue.
// See CSSMathInvert.idl for more information about this class.
class CORE_EXPORT CSSMathInvert : public CSSMathValue {
  WTF_MAKE_NONCOPYABLE(CSSMathInvert);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
  static CSSMathInvert* Create(const CSSNumberish& arg) {
    return Create(CSSNumericValue::FromNumberish(arg));
  }
  // Blink-internal constructor
  static CSSMathInvert* Create(CSSNumericValue* value) {
    return new CSSMathInvert(value,
                             CSSNumericValueType::NegateEntries(value->Type()));
  }

  String getOperator() const final { return "invert"; }

  void value(CSSNumberish& value) { value.SetCSSNumericValue(value_); }
  void setValue(const CSSNumberish& value) {
    value_ = CSSNumericValue::FromNumberish(value);
  }

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kInvertType; }

  void Trace(Visitor* visitor) override {
    visitor->Trace(value_);
    CSSMathValue::Trace(visitor);
  }

 private:
  CSSMathInvert(CSSNumericValue* value, const CSSNumericValueType& type)
      : CSSMathValue(type), value_(value) {}

  Member<CSSNumericValue> value_;
};

}  // namespace blink

#endif  // CSSMathInvert_h
