/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/xla/comparison_util.h"

#include <limits>

#include "tensorflow/compiler/xla/test.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"

namespace xla {
namespace {

// Returns whether the members of `a` and `b` are equal.
bool Equal(const Comparison& a, const Comparison& b) {
  return a.GetDirection() == b.GetDirection() && a.GetType() == b.GetType() &&
         a.GetPrimitiveType() == b.GetPrimitiveType() &&
         a.GetOrder() == b.GetOrder();
}

TEST(Comparison, FloatsDefaultToPartialOrder) {
  EXPECT_EQ(
      Comparison(Comparison::Direction::kGe, PrimitiveType::BF16).GetOrder(),
      Comparison::Order::kPartial);
  EXPECT_EQ(
      Comparison(Comparison::Direction::kGe, PrimitiveType::F32).GetOrder(),
      Comparison::Order::kPartial);
  EXPECT_EQ(
      Comparison(Comparison::Direction::kGe, PrimitiveType::C64).GetOrder(),
      Comparison::Order::kPartial);
}

TEST(Comparison, IntegersDefaultToTotalOrder) {
  EXPECT_EQ(
      Comparison(Comparison::Direction::kGe, PrimitiveType::S32).GetOrder(),
      Comparison::Order::kTotal);
  EXPECT_EQ(
      Comparison(Comparison::Direction::kGe, PrimitiveType::U8).GetOrder(),
      Comparison::Order::kTotal);
  EXPECT_EQ(
      Comparison(Comparison::Direction::kGe, PrimitiveType::PRED).GetOrder(),
      Comparison::Order::kTotal);
}

TEST(Comparison, LegacyConstructorDefaultsToX32) {
  // We expect the legacy constructor to default to a {S,U,F}32 primitive type.
  // This is legacy debt of the previous enumeration Comparison::Type, which
  // attempted to combine ordering and numerical data classification in a single
  // type.
  EXPECT_EQ(Comparison(Comparison::Direction::kGe, Comparison::Type::kFloat)
                .GetPrimitiveType(),
            xla::PrimitiveType::F32);
  EXPECT_EQ(
      Comparison(Comparison::Direction::kGe, Comparison::Type::kFloatTotalOrder)
          .GetPrimitiveType(),
      xla::PrimitiveType::F32);
  EXPECT_EQ(Comparison(Comparison::Direction::kGe, Comparison::Type::kSigned)
                .GetPrimitiveType(),
            xla::PrimitiveType::S32);
  EXPECT_EQ(Comparison(Comparison::Direction::kGe, Comparison::Type::kUnsigned)
                .GetPrimitiveType(),
            xla::PrimitiveType::U32);
}

TEST(Comparison, PartialOrderReflexivity) {
  EXPECT_FALSE(
      Comparison(Comparison::Direction::kEq, PrimitiveType::F32).IsReflexive());
  EXPECT_FALSE(
      Comparison(Comparison::Direction::kLe, PrimitiveType::F32).IsReflexive());
  EXPECT_FALSE(
      Comparison(Comparison::Direction::kLt, PrimitiveType::S32).IsReflexive());
}

TEST(Comparison, TotalOrderReflexivity) {
  EXPECT_TRUE(Comparison(Comparison::Direction::kLe, PrimitiveType::BF16,
                         Comparison::Order::kTotal)
                  .IsReflexive());
  EXPECT_TRUE(Comparison(Comparison::Direction::kGe, PrimitiveType::F32,
                         Comparison::Order::kTotal)
                  .IsReflexive());
  EXPECT_TRUE(
      Comparison(Comparison::Direction::kEq, PrimitiveType::S32).IsReflexive());

  EXPECT_FALSE(Comparison(Comparison::Direction::kNe, PrimitiveType::F32,
                          Comparison::Order::kTotal)
                   .IsReflexive());
  EXPECT_FALSE(Comparison(Comparison::Direction::kLt, PrimitiveType::F64,
                          Comparison::Order::kTotal)
                   .IsReflexive());
}

TEST(Comparison, PartialOrderAntiReflexivity) {
  EXPECT_TRUE(Comparison(Comparison::Direction::kGt, PrimitiveType::F32)
                  .IsAntireflexive());
  EXPECT_TRUE(Comparison(Comparison::Direction::kLt, PrimitiveType::F32,
                         Comparison::Order::kTotal)
                  .IsAntireflexive());
  EXPECT_FALSE(Comparison(Comparison::Direction::kEq, PrimitiveType::F32,
                          Comparison::Order::kTotal)
                   .IsAntireflexive());
}

TEST(Comparison, TotalOrderAntiReflexivity) {
  EXPECT_TRUE(Comparison(Comparison::Direction::kNe, PrimitiveType::BF16,
                         Comparison::Order::kTotal)
                  .IsAntireflexive());
  EXPECT_TRUE(Comparison(Comparison::Direction::kNe, PrimitiveType::S32)
                  .IsAntireflexive());

  EXPECT_FALSE(Comparison(Comparison::Direction::kEq, PrimitiveType::F32,
                          Comparison::Order::kTotal)
                   .IsAntireflexive());
  EXPECT_FALSE(Comparison(Comparison::Direction::kLe, PrimitiveType::F64,
                          Comparison::Order::kTotal)
                   .IsAntireflexive());
  EXPECT_FALSE(Comparison(Comparison::Direction::kLe, PrimitiveType::S8)
                   .IsAntireflexive());
}

TEST(Comparison, Converse) {
  EXPECT_TRUE(Equal(
      Comparison(Comparison::Direction::kLe, PrimitiveType::S8).Converse(),
      Comparison(Comparison::Direction::kGe, PrimitiveType::S8)));

  EXPECT_TRUE(Equal(
      Comparison(Comparison::Direction::kEq, PrimitiveType::U16).Converse(),
      Comparison(Comparison::Direction::kEq, PrimitiveType::U16)));

  EXPECT_TRUE(Equal(
      Comparison(Comparison::Direction::kGt, PrimitiveType::F32).Converse(),
      Comparison(Comparison::Direction::kLt, PrimitiveType::F32)));
}

TEST(Comparison, PartialOrderFloatsShouldNotHaveInverse) {
  EXPECT_FALSE(Comparison(Comparison::Direction::kGt, PrimitiveType::F32)
                   .Inverse()
                   .has_value());
}

TEST(Comparison, Inverse) {
  EXPECT_TRUE(Equal(
      *Comparison(Comparison::Direction::kLe, PrimitiveType::S64).Inverse(),
      Comparison(Comparison::Direction::kGt, PrimitiveType::S64)));

  EXPECT_TRUE(Equal(
      *Comparison(Comparison::Direction::kEq, PrimitiveType::U16).Inverse(),
      Comparison(Comparison::Direction::kNe, PrimitiveType::U16)));

  EXPECT_TRUE(Equal(*Comparison(Comparison::Direction::kGt, PrimitiveType::F32,
                                Comparison::Order::kTotal)
                         .Inverse(),
                    Comparison(Comparison::Direction::kLe, PrimitiveType::F32,
                               Comparison::Order::kTotal)));
}

TEST(Comparison, ToString) {
  EXPECT_EQ(
      Comparison(Comparison::Direction::kLt, PrimitiveType::F32).ToString(),
      ".LT.F32.PARTIALORDER");
  EXPECT_EQ(
      Comparison(Comparison::Direction::kEq, PrimitiveType::S8).ToString(),
      ".EQ.S8.TOTALORDER");

  EXPECT_EQ(Comparison(Comparison::Direction::kGe, PrimitiveType::C128)
                .ToString("_1_", "_2_", "_3_"),
            "_1_GE_2_C128_3_PARTIALORDER");
}

TEST(Comparison, CompareFollowsIEEE754) {
  // Even though this has total ordering, we expect Comparison::Compare to
  // follow the C++ specification for float comparison.
  EXPECT_FALSE(Comparison(Comparison::Direction::kEq, PrimitiveType::F32,
                          Comparison::Order::kTotal)
                   .Compare<float>(std::numeric_limits<float>::quiet_NaN(),
                                   std::numeric_limits<float>::quiet_NaN()));
}

TEST(Comparison, Compare) {
  EXPECT_TRUE(Comparison(Comparison::Direction::kLt, PrimitiveType::F32)
                  .Compare<float>(1.0f, 2.0f));
  EXPECT_FALSE(Comparison(Comparison::Direction::kNe, PrimitiveType::S64)
                   .Compare<int64_t>(1'000'000, 1'000'000));
}

}  // namespace
}  // namespace xla
