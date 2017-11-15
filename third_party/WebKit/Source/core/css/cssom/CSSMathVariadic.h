// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMathVariadic_h
#define CSSMathVariadic_h

#include "core/css/cssom/CSSMathValue.h"
#include "core/css/cssom/CSSNumericArray.h"

namespace blink {

// Represents an arithmetic operation with one or more CSSNumericValues.
class CORE_EXPORT CSSMathVariadic : public CSSMathValue {
  WTF_MAKE_NONCOPYABLE(CSSMathVariadic);

 public:
  CSSNumericArray* values() { return values_.Get(); }
  void setValues(CSSNumericArray* values) { values_ = values; }

  void Trace(Visitor* visitor) override {
    visitor->Trace(values_);
    CSSMathValue::Trace(visitor);
  }

 protected:
  CSSMathVariadic(CSSNumericArray* values, const CSSNumericValueType& type)
      : CSSMathValue(type), values_(values) {}

  template <class BinaryTypeOperation>
  static CSSNumericValueType TypeCheck(const CSSNumericValueVector& values,
                                       BinaryTypeOperation op,
                                       bool& error) {
    error = false;

    CSSNumericValueType final_type = values.front()->Type();
    for (size_t i = 1; i < values.size(); i++) {
      final_type = op(final_type, values[i]->Type(), error);
      if (error)
        return final_type;
    }

    return final_type;
  }

 private:
  Member<CSSNumericArray> values_;
};

}  // namespace blink

#endif  // CSSMathVariadic_h
