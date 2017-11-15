// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSMathProduct.h"

namespace blink {

CSSMathProduct* CSSMathProduct::Create(const HeapVector<CSSNumberish>& args,
                                       ExceptionState& exception_state) {
  if (args.IsEmpty()) {
    exception_state.ThrowDOMException(kSyntaxError, "Arguments can't be empty");
    return nullptr;
  }

  CSSMathProduct* result = Create(CSSNumberishesToNumericValues(args));
  if (!result) {
    exception_state.ThrowTypeError("Incompatible types");
    return nullptr;
  }

  return result;
}

CSSMathProduct* CSSMathProduct::Create(CSSNumericValueVector values) {
  bool error = false;
  CSSNumericValueType final_type =
      CSSMathVariadic::TypeCheck(values, CSSNumericValueType::Multiply, error);
  return error ? nullptr
               : new CSSMathProduct(CSSNumericArray::Create(std::move(values)),
                                    final_type);
}

}  // namespace blink
