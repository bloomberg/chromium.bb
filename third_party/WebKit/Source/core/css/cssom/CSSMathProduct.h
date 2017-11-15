// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMathProduct_h
#define CSSMathProduct_h

#include "core/css/cssom/CSSMathVariadic.h"

namespace blink {

// Represents the product of one or more CSSNumericValues.
// See CSSMathProduct.idl for more information about this class.
class CORE_EXPORT CSSMathProduct final : public CSSMathVariadic {
  WTF_MAKE_NONCOPYABLE(CSSMathProduct);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
  static CSSMathProduct* Create(const HeapVector<CSSNumberish>& args,
                                ExceptionState&);
  // Blink internal-constructor.
  static CSSMathProduct* Create(CSSNumericValueVector);

  String getOperator() const final { return "product"; }

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kProductType; }

 private:
  CSSMathProduct(CSSNumericArray* values, const CSSNumericValueType& type)
      : CSSMathVariadic(values, type) {}
};

}  // namespace blink

#endif  // CSSMathProduct_h
