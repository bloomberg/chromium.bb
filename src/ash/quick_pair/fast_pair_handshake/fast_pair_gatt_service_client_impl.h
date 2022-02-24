// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_IMPL_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_IMPL_H_

#include <stdint.h>

#include <vector>

#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

class BluetoothDevice;
class BluetoothGattConnection;
class BluetoothRemoteGattService;
class BluetoothGattNotifySession;
class BluetoothRemoteGattService;

}  // namespace device

namespace ash {
namespace quick_pair {

class FastPairDataEncryptor;

// This class is responsible for connecting to the Fast Pair GATT service for a
// device and invoking a callback when ready, or when an error is discovered
// during initialization.
class FastPairGattServiceClientImpl : public FastPairGattServiceClient {
 public:
  class Factory {
   public:
    static std::unique_ptr<FastPairGattServiceClient> Create(
        device::BluetoothDevice* device,
        scoped_refptr<device::BluetoothAdapter> adapter,
        base::OnceCallback<void(absl::optional<PairFailure>)>
            on_initialized_callback);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<FastPairGattServiceClient> CreateInstance(
        device::BluetoothDevice* device,
        scoped_refptr<device::BluetoothAdapter> adapter,
        base::OnceCallback<void(absl::optional<PairFailure>)>
            on_initialized_callback) = 0;

   private:
    static Factory* g_test_factory_;
  };

  ~FastPairGattServiceClientImpl() override;

  device::BluetoothRemoteGattService* gatt_service() override;

  bool IsConnected() override;

  void WriteRequestAsync(uint8_t message_type,
                         uint8_t flags,
                         const std::string& provider_address,
                         const std::string& seekers_address,
                         FastPairDataEncryptor* fast_pair_data_encryptor,
                         base::OnceCallback<void(std::vector<uint8_t>,
                                                 absl::optional<PairFailure>)>
                             write_response_callback) override;

  void WritePasskeyAsync(uint8_t message_type,
                         uint32_t passkey,
                         FastPairDataEncryptor* fast_pair_data_encryptor,
                         base::OnceCallback<void(std::vector<uint8_t>,
                                                 absl::optional<PairFailure>)>
                             write_response_callback) override;

  void WriteAccountKey(
      std::array<uint8_t, 16> account_key,
      FastPairDataEncryptor* fast_pair_data_encryptor,
      base::OnceCallback<
          void(absl::optional<device::BluetoothGattService::GattErrorCode>)>
          write_account_key_callback) override;

 private:
  FastPairGattServiceClientImpl(
      device::BluetoothDevice* device,
      scoped_refptr<device::BluetoothAdapter> adapter,
      base::OnceCallback<void(absl::optional<PairFailure>)>
          on_initialized_callback);
  FastPairGattServiceClientImpl(const FastPairGattServiceClientImpl&) = delete;
  FastPairGattServiceClientImpl& operator=(
      const FastPairGattServiceClientImpl&) = delete;

  // Creates a data vector based on parameter information.
  const std::array<uint8_t, kBlockByteSize> CreateRequest(
      uint8_t message_type,
      uint8_t flags,
      const std::string& provider_address,
      const std::string& seekers_address);
  const std::array<uint8_t, kBlockByteSize> CreatePasskeyBlock(
      uint8_t message_type,
      uint32_t passkey);

  // Callback from the adapter's call to create GATT connection.
  void OnGattConnection(
      base::TimeTicks gatt_connection_start_time,
      std::unique_ptr<device::BluetoothGattConnection> gatt_connection,
      absl::optional<device::BluetoothDevice::ConnectErrorCode> error_code);

  // Invokes the initialized callback with the proper PairFailure and clears
  // local state.
  void NotifyInitializedError(PairFailure failure);

  // Invokes the write response callback with the proper PairFailure on a
  // write error.
  void NotifyWriteRequestError(PairFailure failure);
  void NotifyWritePasskeyError(PairFailure failure);
  void NotifyWriteAccountKeyError(
      device::BluetoothGattService::GattErrorCode error);

  void ClearCurrentState();

  // BluetoothAdapter::Observer
  void GattDiscoveryCompleteForService(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattService* service) override;
  void GattCharacteristicValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;

  void FindGattCharacteristicsAndStartNotifySessions();

  std::vector<device::BluetoothRemoteGattCharacteristic*>
  GetCharacteristicsByUUIDs(const device::BluetoothUUID& uuidV1,
                            const device::BluetoothUUID& uuidV2);

  // BluetoothRemoteGattCharacteristic StartNotifySession callbacks
  void OnNotifySession(
      std::unique_ptr<device::BluetoothGattNotifySession> session);
  void OnGattError(PairFailure failure,
                   device::BluetoothGattService::GattErrorCode error);

  // BluetoothRemoteGattCharacteristic WriteRemoteCharacteristic callbacks
  void OnWriteRequest();
  void OnWriteRequestError(device::BluetoothGattService::GattErrorCode error);
  void OnWritePasskey();
  void OnWritePasskeyError(device::BluetoothGattService::GattErrorCode error);
  void OnWriteAccountKey(base::TimeTicks write_account_key_start_time);
  void OnWriteAccountKeyError(
      device::BluetoothGattService::GattErrorCode error);

  base::OneShotTimer gatt_service_discovery_timer_;
  base::OneShotTimer passkey_notify_session_timer_;
  base::OneShotTimer keybased_notify_session_timer_;
  base::OneShotTimer key_based_write_request_timer_;
  base::OneShotTimer passkey_write_request_timer_;

  base::OnceCallback<void(absl::optional<PairFailure>)>
      on_initialized_callback_;
  base::OnceCallback<void(std::vector<uint8_t>, absl::optional<PairFailure>)>
      key_based_write_response_callback_;
  base::OnceCallback<void(std::vector<uint8_t>, absl::optional<PairFailure>)>
      passkey_write_response_callback_;
  base::OnceCallback<void(
      absl::optional<device::BluetoothGattService::GattErrorCode>)>
      write_account_key_callback_;

  std::string device_address_;
  bool is_initialized_ = false;

  // Initial timestamps used to calculate duration to log to metrics.;
  base::TimeTicks notify_keybased_start_time_;
  base::TimeTicks notify_passkey_start_time_;

  device::BluetoothRemoteGattCharacteristic* key_based_characteristic_ =
      nullptr;
  device::BluetoothRemoteGattCharacteristic* passkey_characteristic_ = nullptr;
  device::BluetoothRemoteGattCharacteristic* account_key_characteristic_ =
      nullptr;

  std::vector<std::unique_ptr<device::BluetoothGattNotifySession>>
      bluetooth_gatt_notify_sessions_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  std::unique_ptr<device::BluetoothGattConnection> gatt_connection_;
  device::BluetoothRemoteGattService* gatt_service_ = nullptr;

  base::ScopedObservation<device::BluetoothAdapter,
                          device::BluetoothAdapter::Observer>
      adapter_observation_{this};
  base::WeakPtrFactory<FastPairGattServiceClientImpl> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_GATT_SERVICE_CLIENT_IMPL_H_
