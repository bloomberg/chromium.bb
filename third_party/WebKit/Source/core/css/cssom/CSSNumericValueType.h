// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSNumericValueType_h
#define CSSNumericValueType_h

#include "core/css/CSSPrimitiveValue.h"

#include <array>

namespace blink {

// Represents the type of a CSSNumericValue, which is a map of base types to
// integers, and an associated percent hint.
// https://drafts.css-houdini.org/css-typed-om-1/#numeric-typing
class CORE_EXPORT CSSNumericValueType {
 public:
  enum class BaseType : unsigned {
    kLength,
    kAngle,
    kTime,
    kFrequency,
    kResolution,
    kFlex,
    kPercent,
    kNumBaseTypes
  };

  static constexpr unsigned kNumBaseTypes =
      static_cast<unsigned>(BaseType::kNumBaseTypes);

  explicit CSSNumericValueType(
      CSSPrimitiveValue::UnitType = CSSPrimitiveValue::UnitType::kNumber);

  static CSSNumericValueType NegateExponents(CSSNumericValueType);
  static CSSNumericValueType Add(CSSNumericValueType,
                                 CSSNumericValueType,
                                 bool& error);
  static CSSNumericValueType Multiply(CSSNumericValueType,
                                      CSSNumericValueType,
                                      bool& error);

  int Exponent(BaseType type) const {
    DCHECK_LT(type, BaseType::kNumBaseTypes);
    return exponents_[static_cast<unsigned>(type)];
  }

  void SetExponent(BaseType type, int value) {
    DCHECK_LT(type, BaseType::kNumBaseTypes);
    exponents_[static_cast<unsigned>(type)] = value;
  }

  bool HasPercentHint() const { return has_percent_hint_; }
  BaseType PercentHint() const { return percent_hint_; }
  void ApplyPercentHint(BaseType hint);

 private:
  std::array<int, kNumBaseTypes> exponents_{};  // zero-initialize
  BaseType percent_hint_ = BaseType::kPercent;
  bool has_percent_hint_ = false;
};

}  // namespace blink

#endif  // CSSNumericValueType_h
