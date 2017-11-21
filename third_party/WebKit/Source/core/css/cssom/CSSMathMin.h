// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMathMin_h
#define CSSMathMin_h

#include "base/macros.h"
#include "core/css/cssom/CSSMathVariadic.h"

namespace blink {

// Represents the minimum of one or more CSSNumericValues.
// See CSSMathMin.idl for more information about this class.
class CORE_EXPORT CSSMathMin final : public CSSMathVariadic {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
  static CSSMathMin* Create(const HeapVector<CSSNumberish>& args,
                            ExceptionState&);
  // Blink-internal constructor.
  static CSSMathMin* Create(CSSNumericValueVector);

  String getOperator() const final { return "min"; }

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kMinType; }

 private:
  CSSMathMin(CSSNumericArray* values, const CSSNumericValueType& type)
      : CSSMathVariadic(values, type) {}
  DISALLOW_COPY_AND_ASSIGN(CSSMathMin);
};

}  // namespace blink

#endif  // CSSMathMin_h
