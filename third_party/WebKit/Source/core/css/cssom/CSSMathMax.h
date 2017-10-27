// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMathMax_h
#define CSSMathMax_h

#include "core/css/cssom/CSSMathValue.h"

namespace blink {

// Represents the maximum of one or more CSSNumericValues.
// See CSSMathMax.idl for more information about this class.
class CORE_EXPORT CSSMathMax : public CSSMathValue {
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

    return new CSSMathMax(args);
  }

  String getOperator() const final { return "max"; }

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kMaxType; }

 private:
  explicit CSSMathMax(const HeapVector<CSSNumberish>&) : CSSMathValue() {}
};

}  // namespace blink

#endif  // CSSMathMax_h
