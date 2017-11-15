// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMathValue_h
#define CSSMathValue_h

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSNumericValue.h"

namespace blink {

// Represents mathematical operations, acting as nodes in a tree of
// CSSNumericValues. See CSSMathValue.idl for more information about this class.
class CORE_EXPORT CSSMathValue : public CSSNumericValue {
  WTF_MAKE_NONCOPYABLE(CSSMathValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual String getOperator() const = 0;

  // From CSSNumericValue.
  CSSUnitValue* to(CSSPrimitiveValue::UnitType) const final {
    // TODO(776173): Implement
    return nullptr;
  }
  bool IsUnitValue() const final { return false; }

  // From CSSStyleValue.
  bool ContainsPercent() const final {
    // TODO(776173): Implement
    return false;
  }
  const CSSValue* ToCSSValue() const final {
    // TODO(776173): Implement
    return nullptr;
  }

 protected:
  CSSMathValue(const CSSNumericValueType& type) : CSSNumericValue(type) {}
};

}  // namespace blink

#endif  // CSSMathValue_h
