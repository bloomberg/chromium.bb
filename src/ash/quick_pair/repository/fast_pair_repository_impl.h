// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_IMPL_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_IMPL_H_

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {
class BluetoothDevice;
}

namespace nearby {
namespace fastpair {
class UserReadDevicesResponse;
}  // namespace fastpair
}  // namespace nearby

namespace ash {
namespace quick_pair {

class DeviceMetadataFetcher;
class FastPairImageDecoder;
class FootprintsFetcher;
class SavedDeviceRegistry;

// The entry point for the Repository component in the Quick Pair system,
// responsible for connecting to back-end services.
class FastPairRepositoryImpl : public FastPairRepository {
 public:
  FastPairRepositoryImpl();
  FastPairRepositoryImpl(const FastPairRepositoryImpl&) = delete;
  FastPairRepositoryImpl& operator=(const FastPairRepositoryImpl&) = delete;
  ~FastPairRepositoryImpl() override;

  // FastPairRepository::
  void GetDeviceMetadata(const std::string& hex_model_id,
                         DeviceMetadataCallback callback) override;
  void IsValidModelId(const std::string& hex_model_id,
                      base::OnceCallback<void(bool)> callback) override;
  void CheckAccountKeys(const AccountKeyFilter& account_key_filter,
                        CheckAccountKeysCallback callback) override;
  void AssociateAccountKey(scoped_refptr<Device> device,
                           const std::vector<uint8_t>& account_key) override;
  void DeleteAssociatedDevice(const device::BluetoothDevice* device) override;

 private:
  void CheckAccountKeysImpl(const AccountKeyFilter& account_key_filter,
                            CheckAccountKeysCallback callback,
                            bool refresh_cache_on_miss);
  void OnMetadataFetched(
      const std::string& normalized_model_id,
      DeviceMetadataCallback callback,
      absl::optional<nearby::fastpair::GetObservedDeviceResponse> response);
  void OnImageDecoded(const std::string& normalized_model_id,
                      DeviceMetadataCallback callback,
                      nearby::fastpair::GetObservedDeviceResponse response,
                      gfx::Image image);
  void RetryCheckAccountKeys(
      const AccountKeyFilter& account_key_filter,
      CheckAccountKeysCallback callback,
      absl::optional<nearby::fastpair::UserReadDevicesResponse> user_devices);
  void UpdateUserDevicesCache(
      absl::optional<nearby::fastpair::UserReadDevicesResponse> user_devices);
  void CompleteAccountKeyLookup(CheckAccountKeysCallback callback,
                                const std::vector<uint8_t> account_key,
                                DeviceMetadata* device_metadata);
  void AddToFootprints(const std::string& hex_model_id,
                       const std::string& mac_address,
                       const std::vector<uint8_t>& account_key,
                       DeviceMetadata* metadata);
  void OnAddToFootprintsComplete(const std::string& mac_address,
                                 const std::vector<uint8_t>& account_key,
                                 bool success);

  std::unique_ptr<DeviceMetadataFetcher> device_metadata_fetcher_;
  std::unique_ptr<FootprintsFetcher> footprints_fetcher_;
  std::unique_ptr<FastPairImageDecoder> image_decoder_;
  std::unique_ptr<SavedDeviceRegistry> saved_device_registry_;

  base::flat_map<std::string, std::unique_ptr<DeviceMetadata>> metadata_cache_;
  nearby::fastpair::UserReadDevicesResponse user_devices_cache_;
  base::Time footprints_last_updated_;

  base::WeakPtrFactory<FastPairRepositoryImpl> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_REPOSITORY_IMPL_H_
