// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMathMax_h
#define CSSMathMax_h

#include "core/css/cssom/CSSMathVariadic.h"

namespace blink {

// Represents the maximum of one or more CSSNumericValues.
// See CSSMathMax.idl for more information about this class.
class CORE_EXPORT CSSMathMax final : public CSSMathVariadic {
  WTF_MAKE_NONCOPYABLE(CSSMathMax);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
  static CSSMathMax* Create(const HeapVector<CSSNumberish>& args,
                            ExceptionState& exception_state) {
    if (args.IsEmpty()) {
      exception_state.ThrowDOMException(kSyntaxError,
                                        "Arguments can't be empty");
      return nullptr;
    }

    // TODO(crbug.com/776173): Implement add typing.
    CSSNumericValueType type(CSSPrimitiveValue::UnitType::kNumber);
    return new CSSMathMax(CSSNumericArray::FromNumberishes(args), type);
  }

  String getOperator() const final { return "max"; }

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kMaxType; }

 private:
  CSSMathMax(CSSNumericArray* values, const CSSNumericValueType& type)
      : CSSMathVariadic(values, type) {}
};

}  // namespace blink

#endif  // CSSMathMax_h
