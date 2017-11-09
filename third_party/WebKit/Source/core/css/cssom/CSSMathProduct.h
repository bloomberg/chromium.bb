// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMathProduct_h
#define CSSMathProduct_h

#include "core/css/cssom/CSSMathValue.h"

namespace blink {

// Represents the product of one or more CSSNumericValues.
// See CSSMathProduct.idl for more information about this class.
class CORE_EXPORT CSSMathProduct : public CSSMathValue {
  WTF_MAKE_NONCOPYABLE(CSSMathProduct);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
  static CSSMathProduct* Create(const HeapVector<CSSNumberish>& args,
                                ExceptionState& exception_state) {
    if (args.IsEmpty()) {
      exception_state.ThrowDOMException(kSyntaxError,
                                        "Arguments can't be empty");
      return nullptr;
    }

    // TODO(crbug.com/776173): Implement multiply typing.
    CSSNumericValueType type(CSSPrimitiveValue::UnitType::kNumber);
    return new CSSMathProduct(args, type);
  }

  String getOperator() const final { return "product"; }

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kProductType; }

 private:
  CSSMathProduct(const HeapVector<CSSNumberish>&,
                 const CSSNumericValueType& type)
      : CSSMathValue(type) {}
};

}  // namespace blink

#endif  // CSSMathProduct_h
