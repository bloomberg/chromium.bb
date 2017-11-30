// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSMathProduct.h"

#include "core/css/cssom/CSSUnitValue.h"

namespace blink {

namespace {

CSSNumericSumValue::UnitMap MultiplyUnitMaps(
    CSSNumericSumValue::UnitMap a,
    const CSSNumericSumValue::UnitMap& b) {
  for (const auto& unit_exponent : b) {
    DCHECK_NE(unit_exponent.value, 0);
    const auto old_value =
        a.Contains(unit_exponent.key) ? a.at(unit_exponent.key) : 0;

    // Remove any zero entries
    if (old_value + unit_exponent.value == 0)
      a.erase(unit_exponent.key);
    else
      a.Set(unit_exponent.key, old_value + unit_exponent.value);
  }
  return a;
}

}  // namespace

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

WTF::Optional<CSSNumericSumValue> CSSMathProduct::SumValue() const {
  CSSNumericSumValue sum;
  // Start with the number '1', which is the multiplicative identity.
  sum.terms.push_back(CSSNumericSumValue::Term{1, {}});

  for (const auto& value : NumericValues()) {
    const auto child_sum = value->SumValue();
    if (!child_sum)
      return WTF::nullopt;

    CSSNumericSumValue new_sum;
    for (const auto& a : sum.terms) {
      for (const auto& b : child_sum->terms) {
        new_sum.terms.emplace_back(a.value * b.value,
                                   MultiplyUnitMaps(a.units, b.units));
      }
    }

    sum = new_sum;
  }
  return sum;
}

}  // namespace blink
