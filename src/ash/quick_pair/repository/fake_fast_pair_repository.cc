// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fake_fast_pair_repository.h"

#include "ash/quick_pair/proto/fastpair.pb.h"
#include "base/strings/string_util.h"

namespace ash {
namespace quick_pair {

FakeFastPairRepository::FakeFastPairRepository() : FastPairRepository() {}

FakeFastPairRepository::~FakeFastPairRepository() = default;

void FakeFastPairRepository::SetFakeMetadata(const std::string& hex_model_id,
                                             nearby::fastpair::Device metadata,
                                             gfx::Image image) {
  nearby::fastpair::GetObservedDeviceResponse response;
  response.mutable_device()->CopyFrom(metadata);

  data_[base::ToUpperASCII(hex_model_id)] =
      std::make_unique<DeviceMetadata>(response, image);
}

void FakeFastPairRepository::ClearFakeMetadata(
    const std::string& hex_model_id) {
  data_.erase(base::ToUpperASCII(hex_model_id));
}

void FakeFastPairRepository::SetCheckAccountKeysResult(
    absl::optional<PairingMetadata> result) {
  check_account_key_result_ = result;
}

bool FakeFastPairRepository::HasKeyForDevice(const std::string& mac_address) {
  return saved_account_keys_.contains(mac_address);
}

void FakeFastPairRepository::GetDeviceMetadata(
    const std::string& hex_model_id,
    DeviceMetadataCallback callback) {
  std::string normalized_id = base::ToUpperASCII(hex_model_id);
  if (data_.contains(normalized_id)) {
    std::move(callback).Run(data_[normalized_id].get());
    return;
  }

  std::move(callback).Run(nullptr);
}

void FakeFastPairRepository::IsValidModelId(
    const std::string& hex_model_id,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(data_.contains(base::ToUpperASCII(hex_model_id)));
}

void FakeFastPairRepository::CheckAccountKeys(
    const AccountKeyFilter& account_key_filter,
    CheckAccountKeysCallback callback) {
  std::move(callback).Run(check_account_key_result_);
}

void FakeFastPairRepository::AssociateAccountKey(
    scoped_refptr<Device> device,
    const std::vector<uint8_t>& account_key) {
  saved_account_keys_[device->address] = account_key;
}

void FakeFastPairRepository::DeleteAssociatedDevice(
    const device::BluetoothDevice* device) {}

}  // namespace quick_pair
}  // namespace ash
