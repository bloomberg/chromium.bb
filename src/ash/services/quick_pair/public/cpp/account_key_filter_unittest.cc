// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/quick_pair/public/cpp/account_key_filter.h"
#include <cstdint>
#include <iterator>

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

const std::vector<uint8_t> kAccountKey1{0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                        0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                        0xCC, 0xDD, 0xEE, 0xFF};
const std::vector<uint8_t> kFilterBytes1{0x0A, 0x42, 0x88, 0x10};

const std::vector<uint8_t> kAccountKey2{0x11, 0x11, 0x22, 0x22, 0x33, 0x33,
                                        0x44, 0x44, 0x55, 0x55, 0x66, 0x66,
                                        0x77, 0x77, 0x88, 0x88};
const std::vector<uint8_t> kFilterBytes1And2{0x2F, 0xBA, 0x06, 0x42, 0x00};

const std::vector<uint8_t> kFilterWithBattery{0x4A, 0x00, 0xF0, 0x00};
const std::vector<uint8_t> kBatteryData{0b00110011, 0b01000000, 0b01000000,
                                        0b01000000};

const uint8_t salt = 0xC7;

class AccountKeyFilterTest : public testing::Test {};

TEST_F(AccountKeyFilterTest, EmptyFilter) {
  AccountKeyFilter filter({}, {});
  EXPECT_FALSE(filter.Test(kAccountKey1));
  EXPECT_FALSE(filter.Test(kAccountKey2));
}

TEST_F(AccountKeyFilterTest, EmptyVectorTest) {
  EXPECT_FALSE(
      AccountKeyFilter(kFilterBytes1, {salt}).Test(std::vector<uint8_t>(0)));
}

TEST_F(AccountKeyFilterTest, SingleAccountKey_AsBytes) {
  EXPECT_TRUE(AccountKeyFilter(kFilterBytes1, {salt}).Test(kAccountKey1));
}

TEST_F(AccountKeyFilterTest, TwoAccountKeys_AsBytes) {
  AccountKeyFilter filter(kFilterBytes1And2, {salt});

  EXPECT_TRUE(filter.Test(kAccountKey1));
  EXPECT_TRUE(filter.Test(kAccountKey2));
}

TEST_F(AccountKeyFilterTest, MissingAccountKey) {
  const std::vector<uint8_t> account_key{0x12, 0x22, 0x33, 0x44, 0x55, 0x66,
                                         0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                         0xCC, 0xDD, 0xEE, 0xFF};

  EXPECT_FALSE(AccountKeyFilter(kFilterBytes1, {salt}).Test(account_key));
  EXPECT_FALSE(AccountKeyFilter(kFilterBytes1And2, {salt}).Test(account_key));
}

TEST_F(AccountKeyFilterTest, WithBatteryData) {
  std::vector<uint8_t> salt_values = {salt};
  for (auto& byte : kBatteryData)
    salt_values.push_back(byte);

  EXPECT_TRUE(
      AccountKeyFilter(kFilterWithBattery, salt_values).Test(kAccountKey1));
}

}  // namespace quick_pair
}  // namespace ash
