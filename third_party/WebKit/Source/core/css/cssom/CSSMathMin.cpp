// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSMathMin.h"

namespace blink {

CSSMathMin* CSSMathMin::Create(const HeapVector<CSSNumberish>& args,
                               ExceptionState& exception_state) {
  if (args.IsEmpty()) {
    exception_state.ThrowDOMException(kSyntaxError, "Arguments can't be empty");
    return nullptr;
  }

  CSSMathMin* result = Create(CSSNumberishesToNumericValues(args));
  if (!result) {
    exception_state.ThrowTypeError("Incompatible types");
    return nullptr;
  }

  return result;
}

CSSMathMin* CSSMathMin::Create(CSSNumericValueVector values) {
  bool error = false;
  CSSNumericValueType final_type =
      CSSMathVariadic::TypeCheck(values, CSSNumericValueType::Add, error);
  return error ? nullptr
               : new CSSMathMin(CSSNumericArray::Create(std::move(values)),
                                final_type);
}

}  // namespace blink
