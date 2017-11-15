// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSMathInvert.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSUnitValue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSMathInvert, TypeIsNegationOfArgumentType) {
  CSSUnitValue* value =
      CSSUnitValue::Create(0, CSSPrimitiveValue::UnitType::kPixels);
  const CSSNumericValueType type = CSSMathInvert::Create(value)->Type();

  for (unsigned i = 0; i < CSSNumericValueType::kNumBaseTypes; i++) {
    const auto base_type = static_cast<CSSNumericValueType::BaseType>(i);
    EXPECT_FALSE(type.HasPercentHint());
    if (base_type == CSSNumericValueType::BaseType::kLength)
      EXPECT_EQ(type.Exponent(base_type), -1);
    else
      EXPECT_EQ(type.Exponent(base_type), 0);
  }
}

}  // namespace blink
