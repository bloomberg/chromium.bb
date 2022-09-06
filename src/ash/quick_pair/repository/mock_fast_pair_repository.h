// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_MOCK_FAST_PAIR_REPOSITORY_H_
#define ASH_QUICK_PAIR_REPOSITORY_MOCK_FAST_PAIR_REPOSITORY_H_

#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace quick_pair {

class MockFastPairRepository : public FastPairRepository {
 public:
  MockFastPairRepository();
  MockFastPairRepository(const MockFastPairRepository&) = delete;
  MockFastPairRepository& operator=(const MockFastPairRepository&) = delete;
  ~MockFastPairRepository() override;

  MOCK_METHOD(void,
              GetDeviceMetadata,
              (const std::string& hex_model_id,
               DeviceMetadataCallback callback),
              (override));
  MOCK_METHOD(void,
              CheckAccountKeys,
              (const AccountKeyFilter& account_key_filter,
               CheckAccountKeysCallback callback),
              (override));
  MOCK_METHOD(void,
              AssociateAccountKey,
              (scoped_refptr<Device> device,
               const std::vector<uint8_t>& account_key),
              (override));
  MOCK_METHOD(bool,
              AssociateAccountKeyLocally,
              (scoped_refptr<Device> device),
              (override));
  MOCK_METHOD(void,
              DeleteAssociatedDevice,
              (const std::string& mac_address,
               DeleteAssociatedDeviceCallback callback),
              (override));
  MOCK_METHOD(void,
              FetchDeviceImages,
              (scoped_refptr<Device> device),
              (override));
  MOCK_METHOD(bool,
              PersistDeviceImages,
              (scoped_refptr<Device> device),
              (override));
  MOCK_METHOD(bool,
              EvictDeviceImages,
              (const device::BluetoothDevice* device),
              (override));
  MOCK_METHOD(absl::optional<chromeos::bluetooth_config::DeviceImageInfo>,
              GetImagesForDevice,
              (const std::string& device_id),
              (override));
  MOCK_METHOD(void,
              CheckOptInStatus,
              (CheckOptInStatusCallback callback),
              (override));
  MOCK_METHOD(void,
              UpdateOptInStatus,
              (nearby::fastpair::OptInStatus opt_in_status,
               UpdateOptInStatusCallback callback),
              (override));
  MOCK_METHOD(void,
              DeleteAssociatedDeviceByAccountKey,
              (const std::vector<uint8_t>& account_key,
               DeleteAssociatedDeviceByAccountKeyCallback callback),
              (override));
  MOCK_METHOD(void,
              GetSavedDevices,
              (GetSavedDevicesCallback callback),
              (override));
  MOCK_METHOD(bool,
              IsAccountKeyPairedLocally,
              (const std::vector<uint8_t>& account_key),
              (override));
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_MOCK_FAST_PAIR_REPOSITORY_H_
