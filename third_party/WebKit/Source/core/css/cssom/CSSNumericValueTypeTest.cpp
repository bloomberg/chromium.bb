// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSNumericValueType.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using UnitType = CSSPrimitiveValue::UnitType;
using BaseType = CSSNumericValueType::BaseType;

TEST(CSSNumericValueType, ApplyingPercentHintMovesPowerAndSetsPercentHint) {
  CSSNumericValueType type(UnitType::kPixels);
  type.SetExponent(BaseType::kPercent, 5);
  EXPECT_EQ(5, type.Exponent(BaseType::kPercent));
  EXPECT_EQ(1, type.Exponent(BaseType::kLength));
  EXPECT_FALSE(type.HasPercentHint());

  type.ApplyPercentHint(BaseType::kLength);
  EXPECT_EQ(0, type.Exponent(BaseType::kPercent));
  EXPECT_EQ(6, type.Exponent(BaseType::kLength));
  ASSERT_TRUE(type.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, type.PercentHint());
}

TEST(CSSNumericValueType, AddingDifferentNonNullTypeHintsIsError) {
  // { *length: 0 } + { *time: 0 } == failure
  CSSNumericValueType type1;
  type1.ApplyPercentHint(BaseType::kTime);
  EXPECT_EQ(BaseType::kTime, type1.PercentHint());

  CSSNumericValueType type2;
  type2.ApplyPercentHint(BaseType::kLength);
  EXPECT_EQ(BaseType::kLength, type2.PercentHint());

  bool error = false;
  CSSNumericValueType::Add(type1, type2, error);
  EXPECT_TRUE(error);
}

TEST(CSSNumericValueType, AddingSameTypeRetainsSamePower) {
  // { length: 1 } + { length: 1 } == { length: 1 }
  const CSSNumericValueType type1(UnitType::kPixels);
  const CSSNumericValueType type2(UnitType::kCentimeters);

  bool error = true;
  const auto result = CSSNumericValueType::Add(type1, type2, error);
  EXPECT_FALSE(error);

  EXPECT_EQ(1, result.Exponent(BaseType::kLength));
  EXPECT_FALSE(result.HasPercentHint());
}

TEST(CSSNumericValueType, AddingPercentRetainsPowersAndSetsTypeHint) {
  // { length: 1 } + { percent: 1 } == { *length: 1 }
  const CSSNumericValueType type1(UnitType::kPixels);
  EXPECT_FALSE(type1.HasPercentHint());

  const CSSNumericValueType type2(UnitType::kPercentage);
  EXPECT_FALSE(type2.HasPercentHint());

  bool error = true;
  const auto result = CSSNumericValueType::Add(type1, type2, error);
  EXPECT_FALSE(error);

  EXPECT_EQ(1, result.Exponent(BaseType::kLength));
  ASSERT_TRUE(result.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, result.PercentHint());
}

TEST(CSSNumericValueType,
     AddingSameTypeWithTypeHintRetainsPowersAndSetsTypeHint) {
  // { length: 1 } + { *length: 1 } == { *length: 1 }
  const CSSNumericValueType type1(UnitType::kPixels);
  EXPECT_FALSE(type1.HasPercentHint());

  CSSNumericValueType type2;
  type2.ApplyPercentHint(BaseType::kLength);
  type2.SetExponent(BaseType::kLength, 1);
  ASSERT_TRUE(type2.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, type2.PercentHint());

  bool error = true;
  const auto result = CSSNumericValueType::Add(type1, type2, error);
  EXPECT_FALSE(error);

  EXPECT_EQ(1, result.Exponent(BaseType::kLength));
  ASSERT_TRUE(result.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, result.PercentHint());
}

TEST(CSSNumericValueType,
     AddingSameTypeAndSameTypeHintRetainsPowersAndTypeHint) {
  // { *length: 1 } + { *length: 1 } == { *length: 1 }
  CSSNumericValueType type1;
  type1.ApplyPercentHint(BaseType::kLength);
  type1.SetExponent(BaseType::kLength, 1);
  ASSERT_TRUE(type1.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, type1.PercentHint());

  CSSNumericValueType type2;
  type2.ApplyPercentHint(BaseType::kLength);
  type2.SetExponent(BaseType::kLength, 1);
  ASSERT_TRUE(type2.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, type2.PercentHint());

  bool error = true;
  const auto result = CSSNumericValueType::Add(type1, type2, error);
  EXPECT_FALSE(error);

  EXPECT_EQ(1, result.Exponent(BaseType::kLength));
  ASSERT_TRUE(result.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, result.PercentHint());
}

TEST(CSSNumericValueType, AdditionRequiringApplyingPercentHint) {
  // { *length: 1, time: 2 } + { time: 2, percent: 1 } == { *length: 1, time: 2
  // }
  CSSNumericValueType type1(UnitType::kPercentage);
  type1.ApplyPercentHint(BaseType::kLength);
  type1.SetExponent(BaseType::kTime, 2);
  EXPECT_EQ(1, type1.Exponent(BaseType::kLength));
  EXPECT_EQ(2, type1.Exponent(BaseType::kTime));
  ASSERT_TRUE(type1.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, type1.PercentHint());

  CSSNumericValueType type2(UnitType::kPercentage);
  type2.SetExponent(BaseType::kTime, 2);
  EXPECT_EQ(2, type2.Exponent(BaseType::kTime));
  EXPECT_EQ(1, type2.Exponent(BaseType::kPercent));
  EXPECT_FALSE(type2.HasPercentHint());

  bool error = true;
  const auto result = CSSNumericValueType::Add(type1, type2, error);
  EXPECT_FALSE(error);

  EXPECT_EQ(1, result.Exponent(BaseType::kLength));
  EXPECT_EQ(2, result.Exponent(BaseType::kTime));
  ASSERT_TRUE(result.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, result.PercentHint());
}

TEST(CSSNumericValueType, ImpossibleAdditionWithPercentHints) {
  // { *length: 1, time: 3 } + { time: 2, percent: 2 } == failure
  CSSNumericValueType type1(UnitType::kPercentage);
  type1.ApplyPercentHint(BaseType::kLength);
  type1.SetExponent(BaseType::kTime, 3);
  EXPECT_EQ(1, type1.Exponent(BaseType::kLength));
  EXPECT_EQ(3, type1.Exponent(BaseType::kTime));
  ASSERT_TRUE(type1.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, type1.PercentHint());

  CSSNumericValueType type2;
  type2.SetExponent(BaseType::kPercent, 2);
  type2.SetExponent(BaseType::kTime, 2);
  EXPECT_EQ(2, type2.Exponent(BaseType::kTime));
  EXPECT_EQ(2, type2.Exponent(BaseType::kPercent));
  EXPECT_FALSE(type2.HasPercentHint());

  bool error = false;
  CSSNumericValueType::Add(type1, type2, error);
  EXPECT_TRUE(error);
}

TEST(CSSNumericValueType, MultiplyDifferentNonNullTypeHintsIsError) {
  // { *length: 0 } * { *time: 0 } == failure
  CSSNumericValueType type1;
  type1.ApplyPercentHint(BaseType::kTime);
  EXPECT_EQ(BaseType::kTime, type1.PercentHint());

  CSSNumericValueType type2;
  type2.ApplyPercentHint(BaseType::kLength);
  EXPECT_EQ(BaseType::kLength, type2.PercentHint());

  bool error = false;
  CSSNumericValueType::Multiply(type1, type2, error);
  EXPECT_TRUE(error);
}

TEST(CSSNumericValueType, MultiplyAddsPowersAndSetsPercentHint) {
  // { length: 2, time: 1 } * { length: 3, angle*: 2 } == { length: 5, time: 1,
  // angle: 2 }
  CSSNumericValueType type1;
  type1.SetExponent(BaseType::kLength, 2);
  type1.SetExponent(BaseType::kTime, 1);

  CSSNumericValueType type2;
  type2.ApplyPercentHint(BaseType::kAngle);
  type2.SetExponent(BaseType::kLength, 3);
  type2.SetExponent(BaseType::kAngle, 2);

  bool error = true;
  const auto result = CSSNumericValueType::Multiply(type1, type2, error);
  ASSERT_FALSE(error);

  EXPECT_EQ(5, result.Exponent(BaseType::kLength));
  EXPECT_EQ(1, result.Exponent(BaseType::kTime));
  EXPECT_EQ(2, result.Exponent(BaseType::kAngle));
  ASSERT_TRUE(result.HasPercentHint());
  EXPECT_EQ(BaseType::kAngle, result.PercentHint());
}

TEST(CSSNumericValueType, MatchesBaseTypePercentage) {
  CSSNumericValueType type;
  EXPECT_FALSE(type.MatchesBaseType(BaseType::kLength));
  EXPECT_FALSE(type.MatchesBaseTypePercentage(BaseType::kLength));

  type.SetExponent(BaseType::kLength, 1);
  EXPECT_TRUE(type.MatchesBaseType(BaseType::kLength));
  EXPECT_TRUE(type.MatchesBaseTypePercentage(BaseType::kLength));

  type.SetExponent(BaseType::kLength, 2);
  EXPECT_FALSE(type.MatchesBaseType(BaseType::kLength));
  EXPECT_FALSE(type.MatchesBaseTypePercentage(BaseType::kLength));

  type.SetExponent(BaseType::kLength, 1);
  EXPECT_TRUE(type.MatchesBaseType(BaseType::kLength));
  EXPECT_TRUE(type.MatchesBaseTypePercentage(BaseType::kLength));

  type.ApplyPercentHint(BaseType::kLength);
  EXPECT_FALSE(type.MatchesBaseType(BaseType::kLength));
  EXPECT_TRUE(type.MatchesBaseTypePercentage(BaseType::kLength));
}

TEST(CSSNumericValueType, MatchesPercentage) {
  CSSNumericValueType type;
  EXPECT_FALSE(type.MatchesPercentage());

  type.SetExponent(BaseType::kPercent, 1);
  EXPECT_TRUE(type.MatchesPercentage());

  type.SetExponent(BaseType::kPercent, 2);
  EXPECT_FALSE(type.MatchesPercentage());

  type.ApplyPercentHint(BaseType::kLength);
  EXPECT_FALSE(type.MatchesPercentage());

  type.SetExponent(BaseType::kLength, 0);
  type.SetExponent(BaseType::kPercent, 1);
  EXPECT_TRUE(type.MatchesPercentage());
}

TEST(CSSNumericValueType, MatchesNumberPercentage) {
  CSSNumericValueType type;
  EXPECT_TRUE(type.MatchesNumber());
  EXPECT_TRUE(type.MatchesNumberPercentage());

  type.SetExponent(BaseType::kLength, 1);
  EXPECT_FALSE(type.MatchesNumber());
  EXPECT_FALSE(type.MatchesNumberPercentage());

  type.SetExponent(BaseType::kLength, 0);
  EXPECT_TRUE(type.MatchesNumber());
  EXPECT_TRUE(type.MatchesNumberPercentage());

  type.SetExponent(BaseType::kPercent, 1);
  EXPECT_FALSE(type.MatchesNumber());
  EXPECT_TRUE(type.MatchesNumberPercentage());
}

}  // namespace blink
