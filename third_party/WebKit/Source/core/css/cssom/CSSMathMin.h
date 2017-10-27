// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMathMin_h
#define CSSMathMin_h

#include "core/css/cssom/CSSMathValue.h"

namespace blink {

// Represents the minimum of one or more CSSNumericValues.
// See CSSMathMin.idl for more information about this class.
class CORE_EXPORT CSSMathMin : public CSSMathValue {
  WTF_MAKE_NONCOPYABLE(CSSMathMin);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
  static CSSMathMin* Create(const HeapVector<CSSNumberish>& args,
                            ExceptionState& exception_state) {
    if (args.IsEmpty()) {
      exception_state.ThrowDOMException(kSyntaxError,
                                        "Arguments can't be empty");
      return nullptr;
    }

    return new CSSMathMin(args);
  }

  String getOperator() const final { return "min"; }

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kMinType; }

 private:
  explicit CSSMathMin(const HeapVector<CSSNumberish>&) : CSSMathValue() {}
};

}  // namespace blink

#endif  // CSSMathMin_h
