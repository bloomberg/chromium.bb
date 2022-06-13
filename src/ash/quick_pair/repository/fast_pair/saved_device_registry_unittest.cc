// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/saved_device_registry.h"

#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFirstSavedMacAddress[] = "00:11:22:33:44";
constexpr char kSecondSavedMacAddress[] = "AA:11:BB:33:CC";
constexpr char kNotSavedMacAddress[] = "FF:FF:FF:FF:FF";
const std::vector<uint8_t> kAccountKey1{0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                        0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                        0xCC, 0xDD, 0xEE, 0xFF};
const std::vector<uint8_t> kAccountKey2{0x11, 0x11, 0x22, 0x22, 0x33, 0x33,
                                        0x44, 0x44, 0x55, 0x55, 0x66, 0x66,
                                        0x77, 0x77, 0x88, 0x88};

}  // namespace

namespace ash {
namespace quick_pair {

class SavedDeviceRegistryTest : public testing::Test {
 public:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    SavedDeviceRegistry::RegisterProfilePrefs(pref_service_->registry());

    browser_delegate_ = std::make_unique<MockQuickPairBrowserDelegate>();
    ON_CALL(*browser_delegate_, GetActivePrefService())
        .WillByDefault(testing::Return(pref_service_.get()));

    saved_device_registry_ = std::make_unique<SavedDeviceRegistry>();
  }

 protected:
  std::unique_ptr<MockQuickPairBrowserDelegate> browser_delegate_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<SavedDeviceRegistry> saved_device_registry_;
};

TEST_F(SavedDeviceRegistryTest, ValidLookup) {
  saved_device_registry_->SaveAccountKey(kFirstSavedMacAddress, kAccountKey1);
  saved_device_registry_->SaveAccountKey(kSecondSavedMacAddress, kAccountKey2);

  auto first = saved_device_registry_->GetAccountKey(kFirstSavedMacAddress);
  auto second = saved_device_registry_->GetAccountKey(kSecondSavedMacAddress);

  ASSERT_EQ(kAccountKey1, *first);
  ASSERT_EQ(kAccountKey2, *second);
}

TEST_F(SavedDeviceRegistryTest, InvalidLookup) {
  saved_device_registry_->SaveAccountKey(kFirstSavedMacAddress, kAccountKey1);

  auto invalid_result =
      saved_device_registry_->GetAccountKey(kNotSavedMacAddress);
  ASSERT_EQ(absl::nullopt, invalid_result);
}

}  // namespace quick_pair
}  // namespace ash
