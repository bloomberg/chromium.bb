// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_PAIRER_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_PAIRER_H_

#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_gatt_service_client.h"
#include "ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "device/bluetooth/bluetooth_device.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

class BluetoothAdapter;

}  // namespace device

namespace ash {
namespace quick_pair {

struct Device;
enum class AccountKeyFailure;
enum class PairFailure;
class FastPairDataEncryptor;

// A FastPairPairer instance is responsible for the pairing procedure to a
// single device.  Pairing begins on instantiation.
class FastPairPairer : public device::BluetoothDevice::PairingDelegate,
                       public device::BluetoothAdapter::Observer {
 public:
  FastPairPairer(
      scoped_refptr<device::BluetoothAdapter> adapter,
      scoped_refptr<Device> device,
      base::OnceCallback<void(scoped_refptr<Device>)> paired_callback,
      base::OnceCallback<void(scoped_refptr<Device>, PairFailure)>
          pair_failed_callback,
      base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>
          account_key_failure_callback,
      base::OnceCallback<void(scoped_refptr<Device>)>
          pairing_procedure_complete);
  FastPairPairer(const FastPairPairer&) = delete;
  FastPairPairer& operator=(const FastPairPairer&) = delete;
  FastPairPairer(FastPairPairer&&) = delete;
  FastPairPairer& operator=(FastPairPairer&&) = delete;
  ~FastPairPairer() override;

 private:
  // device::BluetoothDevice::PairingDelegate
  void RequestPinCode(device::BluetoothDevice* device) override;
  void ConfirmPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void RequestPasskey(device::BluetoothDevice* device) override;
  void DisplayPinCode(device::BluetoothDevice* device,
                      const std::string& pincode) override;
  void DisplayPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void KeysEntered(device::BluetoothDevice* device, uint32_t entered) override;
  void AuthorizePairing(device::BluetoothDevice* device) override;

  // device::BluetoothAdapter::Obserer
  void DevicePairedChanged(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device,
                           bool new_paired_status) override;

  // FastPairGattServiceClientImpl::Factory::Create callback
  void OnGattClientInitializedCallback(absl::optional<PairFailure> failure);

  // FastPairDataEncryptor::Factory::CreateAsync callback
  // Once the data encryptor is created, triggers a WriteRequestAsync in the
  // client to be encrypted with the DataEncryptor and written to the device.
  void OnDataEncryptorCreateAsync(
      std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor);

  // FastPairGattServiceClient::WriteRequest callback
  void OnWriteResponse(std::vector<uint8_t> response_bytes,
                       absl::optional<PairFailure> failure);

  // FastPairDataEncryptor::ParseDecryptedResponse callback
  void OnParseDecryptedResponse(
      const absl::optional<DecryptedResponse>& response);

  // device::BluetoothDevice::Pair callback
  void OnPairConnected(
      absl::optional<device::BluetoothDevice::ConnectErrorCode> error);

  // device::BluetoothAdapter::ConnectDevice callbacks
  void OnConnectDevice(device::BluetoothDevice* device);
  void OnConnectError();

  // FastPairGattServiceClient::WritePasskey callback
  void OnPasskeyResponse(std::vector<uint8_t> response_bytes,
                         absl::optional<PairFailure> failure);

  // FastPairDataEncryptor::ParseDecryptedPasskey callback
  void OnParseDecryptedPasskey(const absl::optional<DecryptedPasskey>& passkey);

  // Creates a 16-byte array of random bytes with a first byte of 0x04 to
  // signal Fast Pair account key, and then writes to the device.
  void SendAccountKey();

  // FastPairDataEncryptor::WriteAccountKey callback
  void OnWriteAccountKey(
      std::array<uint8_t, 16> account_key,
      absl::optional<device::BluetoothGattService::GattErrorCode> error);

  uint32_t expected_passkey_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  scoped_refptr<Device> device_;
  std::string pairing_device_address_;
  base::OnceCallback<void(scoped_refptr<Device>)> paired_callback_;
  base::OnceCallback<void(scoped_refptr<Device>, PairFailure)>
      pair_failed_callback_;
  base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>
      account_key_failure_callback_;
  base::OnceCallback<void(scoped_refptr<Device>)> pairing_procedure_complete_;
  std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor_;
  std::unique_ptr<FastPairGattServiceClient> fast_pair_gatt_service_client_;
  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};
  base::WeakPtrFactory<FastPairPairer> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_PAIRER_H_
