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
  type.SetEntry(BaseType::kPercent, 5);
  EXPECT_EQ(5, type.GetEntry(BaseType::kPercent));
  EXPECT_EQ(1, type.GetEntry(BaseType::kLength));
  EXPECT_FALSE(type.HasPercentHint());

  type.ApplyPercentHint(BaseType::kLength);
  EXPECT_EQ(0, type.GetEntry(BaseType::kPercent));
  EXPECT_EQ(6, type.GetEntry(BaseType::kLength));
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

  EXPECT_EQ(1, result.GetEntry(BaseType::kLength));
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

  EXPECT_EQ(1, result.GetEntry(BaseType::kLength));
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
  type2.SetEntry(BaseType::kLength, 1);
  ASSERT_TRUE(type2.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, type2.PercentHint());

  bool error = true;
  const auto result = CSSNumericValueType::Add(type1, type2, error);
  EXPECT_FALSE(error);

  EXPECT_EQ(1, result.GetEntry(BaseType::kLength));
  ASSERT_TRUE(result.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, result.PercentHint());
}

TEST(CSSNumericValueType,
     AddingSameTypeAndSameTypeHintRetainsPowersAndTypeHint) {
  // { *length: 1 } + { *length: 1 } == { *length: 1 }
  CSSNumericValueType type1;
  type1.ApplyPercentHint(BaseType::kLength);
  type1.SetEntry(BaseType::kLength, 1);
  ASSERT_TRUE(type1.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, type1.PercentHint());

  CSSNumericValueType type2;
  type2.ApplyPercentHint(BaseType::kLength);
  type2.SetEntry(BaseType::kLength, 1);
  ASSERT_TRUE(type2.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, type2.PercentHint());

  bool error = true;
  const auto result = CSSNumericValueType::Add(type1, type2, error);
  EXPECT_FALSE(error);

  EXPECT_EQ(1, result.GetEntry(BaseType::kLength));
  ASSERT_TRUE(result.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, result.PercentHint());
}

TEST(CSSNumericValueType, AdditionRequiringApplyingPercentHint) {
  // { *length: 1, time: 2 } + { time: 2, percent: 1 } == { *length: 1, time: 2
  // }
  CSSNumericValueType type1(UnitType::kPercentage);
  type1.ApplyPercentHint(BaseType::kLength);
  type1.SetEntry(BaseType::kTime, 2);
  EXPECT_EQ(1, type1.GetEntry(BaseType::kLength));
  EXPECT_EQ(2, type1.GetEntry(BaseType::kTime));
  ASSERT_TRUE(type1.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, type1.PercentHint());

  CSSNumericValueType type2(UnitType::kPercentage);
  type2.SetEntry(BaseType::kTime, 2);
  EXPECT_EQ(2, type2.GetEntry(BaseType::kTime));
  EXPECT_EQ(1, type2.GetEntry(BaseType::kPercent));
  EXPECT_FALSE(type2.HasPercentHint());

  bool error = true;
  const auto result = CSSNumericValueType::Add(type1, type2, error);
  EXPECT_FALSE(error);

  EXPECT_EQ(1, result.GetEntry(BaseType::kLength));
  EXPECT_EQ(2, result.GetEntry(BaseType::kTime));
  ASSERT_TRUE(result.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, result.PercentHint());
}

TEST(CSSNumericValueType, ImpossibleAdditionWithPercentHints) {
  // { *length: 1, time: 3 } + { time: 2, percent: 2 } == failure
  CSSNumericValueType type1(UnitType::kPercentage);
  type1.ApplyPercentHint(BaseType::kLength);
  type1.SetEntry(BaseType::kTime, 3);
  EXPECT_EQ(1, type1.GetEntry(BaseType::kLength));
  EXPECT_EQ(3, type1.GetEntry(BaseType::kTime));
  ASSERT_TRUE(type1.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, type1.PercentHint());

  CSSNumericValueType type2;
  type2.SetEntry(BaseType::kPercent, 2);
  type2.SetEntry(BaseType::kTime, 2);
  EXPECT_EQ(2, type2.GetEntry(BaseType::kTime));
  EXPECT_EQ(2, type2.GetEntry(BaseType::kPercent));
  EXPECT_FALSE(type2.HasPercentHint());

  bool error = false;
  CSSNumericValueType::Add(type1, type2, error);
  EXPECT_TRUE(error);
}

}  // namespace blink
