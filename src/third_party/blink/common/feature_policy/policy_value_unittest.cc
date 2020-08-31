// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/feature_policy/policy_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/feature_policy/policy_value.mojom.h"

namespace blink {

class PolicyValueTest : public testing::Test {};

TEST_F(PolicyValueTest, TestCanCreateBoolValues) {
  PolicyValue false_value(false);
  PolicyValue true_value(true);
  PolicyValue min_value(
      PolicyValue::CreateMinPolicyValue(mojom::PolicyValueType::kBool));
  PolicyValue max_value(
      PolicyValue::CreateMaxPolicyValue(mojom::PolicyValueType::kBool));
  EXPECT_EQ(false_value.BoolValue(), false);
  EXPECT_EQ(true_value.BoolValue(), true);
  EXPECT_EQ(min_value.BoolValue(), false);
  EXPECT_EQ(max_value.BoolValue(), true);
}

TEST_F(PolicyValueTest, TestCanModifyBoolValues) {
  PolicyValue initially_false_value(false);
  PolicyValue initially_true_value(true);
  initially_false_value.SetBoolValue(true);
  initially_true_value.SetBoolValue(false);
  EXPECT_EQ(initially_false_value.BoolValue(), true);
  EXPECT_EQ(initially_true_value.BoolValue(), false);

  initially_true_value.SetToMax();
  EXPECT_EQ(initially_true_value.BoolValue(), true);
  initially_true_value.SetToMin();
  EXPECT_EQ(initially_true_value.BoolValue(), false);
}

TEST_F(PolicyValueTest, TestCanCompareBoolValues) {
  PolicyValue false_value(false);
  PolicyValue true_value(true);

  EXPECT_FALSE(false_value < false_value);
  EXPECT_TRUE(false_value <= false_value);
  EXPECT_TRUE(false_value == false_value);
  EXPECT_FALSE(false_value != false_value);
  EXPECT_TRUE(false_value >= false_value);
  EXPECT_FALSE(false_value > false_value);

  EXPECT_TRUE(false_value < true_value);
  EXPECT_TRUE(false_value <= true_value);
  EXPECT_FALSE(false_value == true_value);
  EXPECT_TRUE(false_value != true_value);
  EXPECT_FALSE(false_value >= true_value);
  EXPECT_FALSE(false_value > true_value);

  EXPECT_FALSE(true_value < false_value);
  EXPECT_FALSE(true_value <= false_value);
  EXPECT_FALSE(true_value == false_value);
  EXPECT_TRUE(true_value != false_value);
  EXPECT_TRUE(true_value >= false_value);
  EXPECT_TRUE(true_value > false_value);

  EXPECT_FALSE(true_value < true_value);
  EXPECT_TRUE(true_value <= true_value);
  EXPECT_TRUE(true_value == true_value);
  EXPECT_FALSE(true_value != true_value);
  EXPECT_TRUE(true_value >= true_value);
  EXPECT_FALSE(true_value > true_value);
}

TEST_F(PolicyValueTest, TestCanCreateDoubleValues) {
  PolicyValue zero_value(0.0);
  PolicyValue one_value(1.0);
  PolicyValue min_value(
      PolicyValue::CreateMinPolicyValue(mojom::PolicyValueType::kDecDouble));
  PolicyValue max_value(
      PolicyValue::CreateMaxPolicyValue(mojom::PolicyValueType::kDecDouble));
  EXPECT_EQ(zero_value.DoubleValue(), 0.0);
  EXPECT_EQ(one_value.DoubleValue(), 1.0);
  EXPECT_EQ(min_value.DoubleValue(), 0.0);
  EXPECT_EQ(max_value.DoubleValue(), std::numeric_limits<double>::infinity());
}

TEST_F(PolicyValueTest, TestCanModifyDoubleValues) {
  PolicyValue initially_zero_value(0.0);
  initially_zero_value.SetDoubleValue(1.0);
  EXPECT_EQ(initially_zero_value.DoubleValue(), 1.0);
  initially_zero_value.SetToMax();
  EXPECT_EQ(initially_zero_value.DoubleValue(),
            std::numeric_limits<double>::infinity());
  initially_zero_value.SetToMin();
  EXPECT_EQ(initially_zero_value.DoubleValue(), 0.0);
}

TEST_F(PolicyValueTest, TestCanCompareDoubleValues) {
  PolicyValue low_value(1.0);
  PolicyValue high_value(2.0);

  EXPECT_FALSE(low_value < low_value);
  EXPECT_TRUE(low_value <= low_value);
  EXPECT_TRUE(low_value == low_value);
  EXPECT_FALSE(low_value != low_value);
  EXPECT_TRUE(low_value >= low_value);
  EXPECT_FALSE(low_value > low_value);

  EXPECT_TRUE(low_value < high_value);
  EXPECT_TRUE(low_value <= high_value);
  EXPECT_FALSE(low_value == high_value);
  EXPECT_TRUE(low_value != high_value);
  EXPECT_FALSE(low_value >= high_value);
  EXPECT_FALSE(low_value > high_value);

  EXPECT_FALSE(high_value < low_value);
  EXPECT_FALSE(high_value <= low_value);
  EXPECT_FALSE(high_value == low_value);
  EXPECT_TRUE(high_value != low_value);
  EXPECT_TRUE(high_value >= low_value);
  EXPECT_TRUE(high_value > low_value);

  EXPECT_FALSE(high_value < high_value);
  EXPECT_TRUE(high_value <= high_value);
  EXPECT_TRUE(high_value == high_value);
  EXPECT_FALSE(high_value != high_value);
  EXPECT_TRUE(high_value >= high_value);
  EXPECT_FALSE(high_value > high_value);
}

}  // namespace blink
