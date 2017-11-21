// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMathSum_h
#define CSSMathSum_h

#include "base/macros.h"
#include "core/css/cssom/CSSMathVariadic.h"

namespace blink {

// Represents the sum of one or more CSSNumericValues.
// See CSSMathSum.idl for more information about this class.
class CORE_EXPORT CSSMathSum final : public CSSMathVariadic {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // The constructor defined in the IDL.
  static CSSMathSum* Create(const HeapVector<CSSNumberish>& args,
                            ExceptionState&);
  // Blink-internal constructor.
  static CSSMathSum* Create(CSSNumericValueVector);

  String getOperator() const final { return "sum"; }

  // From CSSStyleValue.
  StyleValueType GetType() const final { return CSSStyleValue::kSumType; }

 private:
  CSSMathSum(CSSNumericArray* values, const CSSNumericValueType& type)
      : CSSMathVariadic(values, type) {}
  DISALLOW_COPY_AND_ASSIGN(CSSMathSum);
};

}  // namespace blink

#endif  // CSSMathSum_h
