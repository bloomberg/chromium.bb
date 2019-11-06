// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/chromeos/bluetooth_utils.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothUtilsTest : public testing::Test {
 protected:
  BluetoothUtilsTest() = default;

  void SetLongTermKeys(const std::string& keys) {
    feature_list_.InitAndEnableFeatureWithParameters(
        chromeos::features::kBlueZLongTermKeyBlocklist,
        {{chromeos::features::kBlueZLongTermKeyBlocklistParamName, keys}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothUtilsTest);
};

TEST_F(BluetoothUtilsTest, TestListIncludesBadLtks) {
  // One nibble too long, one nibble too short, and one nibble just right.
  std::string hex_key_1 = "000000000000000000000000000012345";
  std::string hex_key_2 = "0000000000000000000000000000123";
  std::string hex_key_3 = "00000000000000000000000000001234";
  SetLongTermKeys(hex_key_1 + ',' + hex_key_2 + ',' + hex_key_3);

  std::vector<std::vector<uint8_t>> expected_array;
  std::vector<uint8_t> expected_key = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x12, 0x34};
  expected_array.push_back(expected_key);

  EXPECT_EQ(expected_array, device::GetBlockedLongTermKeys());
}

TEST_F(BluetoothUtilsTest, TestListIncludesNonHexInput) {
  std::string hex_key_1 = "bad00input00but00correct00length";
  std::string hex_key_2 = "00000000000000000000000000001234";
  SetLongTermKeys(hex_key_1 + ',' + hex_key_2);

  std::vector<std::vector<uint8_t>> expected_array;
  std::vector<uint8_t> expected_key = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x12, 0x34};
  expected_array.push_back(expected_key);

  EXPECT_EQ(expected_array, device::GetBlockedLongTermKeys());
}

TEST_F(BluetoothUtilsTest, TestEmptyList) {
  SetLongTermKeys("");

  std::vector<std::vector<uint8_t>> expected_array;

  EXPECT_EQ(expected_array, device::GetBlockedLongTermKeys());
}

TEST_F(BluetoothUtilsTest, TestOneElementList) {
  std::string hex_key_1 = "012300004567000089ab0000cdef0000";
  std::vector<uint8_t> expected_key_1 = {0x01, 0x23, 0x00, 0x00, 0x45, 0x67,
                                         0x00, 0x00, 0x89, 0xab, 0x00, 0x00,
                                         0xcd, 0xef, 0x00, 0x00};

  SetLongTermKeys(hex_key_1);

  std::vector<std::vector<uint8_t>> expected_array;
  expected_array.push_back(expected_key_1);

  EXPECT_EQ(expected_array, device::GetBlockedLongTermKeys());
}

TEST_F(BluetoothUtilsTest, TestMultipleElementList) {
  std::string hex_key_1 = "012300004567000089ab0000cdef0000";
  std::vector<uint8_t> expected_key_1 = {0x01, 0x23, 0x00, 0x00, 0x45, 0x67,
                                         0x00, 0x00, 0x89, 0xab, 0x00, 0x00,
                                         0xcd, 0xef, 0x00, 0x00};

  std::string hex_key_2 = "00001111222233334444555566667777";
  std::vector<uint8_t> expected_key_2 = {0x00, 0x00, 0x11, 0x11, 0x22, 0x22,
                                         0x33, 0x33, 0x44, 0x44, 0x55, 0x55,
                                         0x66, 0x66, 0x77, 0x77};

  std::string hex_key_3 = "88889999aaaabbbbccccddddeeeeffff";
  std::vector<uint8_t> expected_key_3 = {0x88, 0x88, 0x99, 0x99, 0xaa, 0xaa,
                                         0xbb, 0xbb, 0xcc, 0xcc, 0xdd, 0xdd,
                                         0xee, 0xee, 0xff, 0xff};

  SetLongTermKeys(hex_key_1 + ',' + hex_key_2 + ',' + hex_key_3);

  std::vector<std::vector<uint8_t>> expected_array;
  expected_array.push_back(expected_key_1);
  expected_array.push_back(expected_key_2);
  expected_array.push_back(expected_key_3);

  EXPECT_EQ(expected_array, device::GetBlockedLongTermKeys());
}

}  // namespace device
