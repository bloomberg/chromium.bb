// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMathSum_h
#define CSSMathSum_h

#include "core/css/cssom/CSSMathValue.h"

namespace blink {

// Represents the sum of one or more CSSNumericValues.
// See CSSMathSum.idl for more information about this class.
class CORE_EXPORT CSSMathSum final : public CSSMathValue {
  WTF_MAKE_NONCOPYABLE(CSSMathSum);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
  static CSSMathSum* Create(const HeapVector<CSSNumberish>& args,
                            ExceptionState& exception_state) {
    if (args.IsEmpty()) {
      exception_state.ThrowDOMException(kSyntaxError,
                                        "Arguments can't be empty");
      return nullptr;
    }

    // TODO(crbug.com/776173): Implement add typing.
    CSSNumericValueType type(CSSPrimitiveValue::UnitType::kNumber);
    return new CSSMathSum(args, type);
  }

  String getOperator() const final { return "sum"; }

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kSumType; }

 private:
  CSSMathSum(const HeapVector<CSSNumberish>&, const CSSNumericValueType& type)
      : CSSMathValue(type) {}
};

}  // namespace blink

#endif  // CSSMathSum_h
