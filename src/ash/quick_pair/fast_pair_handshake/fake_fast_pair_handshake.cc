// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "base/callback.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {
namespace quick_pair {

FakeFastPairHandshake::FakeFastPairHandshake(
    scoped_refptr<device::BluetoothAdapter> adapter,
    scoped_refptr<Device> device,
    OnCompleteCallback on_complete,
    std::unique_ptr<FastPairDataEncryptor> data_encryptor,
    std::unique_ptr<FastPairGattServiceClient> gatt_service_client)
    : FastPairHandshake(std::move(adapter),
                        std::move(device),
                        std::move(on_complete),
                        std::move(data_encryptor),
                        std::move(gatt_service_client)) {}

FakeFastPairHandshake::~FakeFastPairHandshake() = default;

void FakeFastPairHandshake::InvokeCallback(
    absl::optional<PairFailure> failure) {
  bool has_failure = failure.has_value();
  std::move(on_complete_callback_).Run(device_, std::move(failure));
  completed_successfully_ = !has_failure;
}

}  // namespace quick_pair
}  // namespace ash
