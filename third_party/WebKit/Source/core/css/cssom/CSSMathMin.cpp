// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSMathMin.h"

#include "core/css/cssom/CSSNumericSumValue.h"

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

WTF::Optional<CSSNumericSumValue> CSSMathMin::SumValue() const {
  auto cur_min = NumericValues()[0]->SumValue();
  if (!cur_min || cur_min->terms.size() != 1)
    return WTF::nullopt;

  for (const auto& value : NumericValues()) {
    const auto child_sum = value->SumValue();
    if (!child_sum || child_sum->terms.size() != 1 ||
        child_sum->terms[0].units != cur_min->terms[0].units)
      return WTF::nullopt;

    if (child_sum->terms[0].value < cur_min->terms[0].value)
      cur_min = child_sum;
  }
  return cur_min;
}

}  // namespace blink
