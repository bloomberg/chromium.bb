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

  static CSSNumericValueType NegateEntries(CSSNumericValueType);
  static CSSNumericValueType Add(CSSNumericValueType,
                                 CSSNumericValueType,
                                 bool& error);

  int GetEntry(BaseType type) const {
    DCHECK_LT(type, BaseType::kNumBaseTypes);
    return entries_[static_cast<unsigned>(type)];
  }

  void SetEntry(BaseType type, int value) {
    DCHECK_LT(type, BaseType::kNumBaseTypes);
    entries_[static_cast<unsigned>(type)] = value;
  }

  bool HasPercentHint() const { return has_percent_hint_; }
  BaseType PercentHint() const { return percent_hint_; }
  void ApplyPercentHint(BaseType hint);

 private:
  std::array<int, kNumBaseTypes> entries_{};  // zero-initialize
  BaseType percent_hint_ = BaseType::kPercent;
  bool has_percent_hint_ = false;
};

}  // namespace blink

#endif  // CSSNumericValueType_h
