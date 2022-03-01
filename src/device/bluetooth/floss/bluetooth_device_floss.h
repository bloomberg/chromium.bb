// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_DEVICE_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_DEVICE_FLOSS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/bluetooth_pairing_floss.h"
#include "device/bluetooth/floss/floss_adapter_client.h"

namespace floss {

class BluetoothAdapterFloss;
struct Error;
struct FlossDeviceId;

// BluetoothDeviceFloss implements device::BluetoothDevice for platforms using
// Floss (Linux front-end for Fluoride). Objects of this type should be managed
// by BluetoothAdapterFloss, which also manages device lifetimes.
//
// This class is not thread-safe but is only called from the UI thread.
class DEVICE_BLUETOOTH_EXPORT BluetoothDeviceFloss
    : public device::BluetoothDevice {
 public:
  BluetoothDeviceFloss(const BluetoothDeviceFloss&) = delete;
  BluetoothDeviceFloss& operator=(const BluetoothDeviceFloss&) = delete;

  BluetoothDeviceFloss(BluetoothAdapterFloss* adapter,
                       const FlossDeviceId& device);
  ~BluetoothDeviceFloss() override;

  // BluetoothDevice override
  uint32_t GetBluetoothClass() const override;
  device::BluetoothTransport GetType() const override;
  std::string GetAddress() const override;
  AddressType GetAddressType() const override;
  VendorIDSource GetVendorIDSource() const override;
  uint16_t GetVendorID() const override;
  uint16_t GetProductID() const override;
  uint16_t GetDeviceID() const override;
  uint16_t GetAppearance() const override;
  absl::optional<std::string> GetName() const override;
  bool IsPaired() const override;
  bool IsConnected() const override;
  bool IsGattConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
  UUIDSet GetUUIDs() const override;
  absl::optional<int8_t> GetInquiryRSSI() const override;
  absl::optional<int8_t> GetInquiryTxPower() const override;
  bool ExpectingPinCode() const override;
  bool ExpectingPasskey() const override;
  bool ExpectingConfirmation() const override;
  void GetConnectionInfo(ConnectionInfoCallback callback) override;
  void SetConnectionLatency(ConnectionLatency connection_latency,
                            base::OnceClosure callback,
                            ErrorCallback error_callback) override;
  void Connect(device::BluetoothDevice::PairingDelegate* pairing_delegate,
               ConnectCallback callback) override;
  void SetPinCode(const std::string& pincode) override;
  void SetPasskey(uint32_t passkey) override;
  void ConfirmPairing() override;
  void RejectPairing() override;
  void CancelPairing() override;
  void Disconnect(base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  void Forget(base::OnceClosure callback,
              ErrorCallback error_callback) override;
  void ConnectToService(const device::BluetoothUUID& uuid,
                        ConnectToServiceCallback callback,
                        ConnectToServiceErrorCallback error_callback) override;
  void ConnectToServiceInsecurely(
      const device::BluetoothUUID& uuid,
      ConnectToServiceCallback callback,
      ConnectToServiceErrorCallback error_callback) override;
  std::unique_ptr<device::BluetoothGattConnection>
  CreateBluetoothGattConnectionObject() override;
  void SetGattServicesDiscoveryComplete(bool complete) override;
  bool IsGattServicesDiscoveryComplete() const override;
  void Pair(device::BluetoothDevice::PairingDelegate* pairing_delegate,
            ConnectCallback callback) override;
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  void ExecuteWrite(base::OnceClosure callback,
                    ExecuteWriteErrorCallback error_callback) override;
  void AbortWrite(base::OnceClosure callback,
                  AbortWriteErrorCallback error_callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

  FlossDeviceId AsFlossDeviceId() const;
  void SetName(const std::string& name);
  void SetBondState(FlossAdapterClient::BondState bond_state);
  void SetIsConnected(bool is_connected);
  void ConnectAllEnabledProfiles();
  void ResetPairing();

  BluetoothPairingFloss* pairing() const { return pairing_.get(); }

 protected:
  // BluetoothDevice override
  void CreateGattConnectionImpl(
      absl::optional<device::BluetoothUUID> service_uuid) override;
  void DisconnectGatt() override;

 private:
  // Method for connecting to this device.
  void ConnectInternal(ConnectCallback callback);
  void OnConnect(ConnectCallback callback);
  void OnConnectError(ConnectCallback callback, const Error& error);

  // Used if a Connect() is called but requires Pairing.
  void OnPairDuringConnect(ConnectCallback callback);
  void OnPairDuringConnectError(ConnectCallback callback, const Error& error);

  void OnDisconnect(base::OnceClosure callback);
  void OnDisconnectError(ErrorCallback error_callback, const Error& error);

  void OnPair(ConnectCallback callback);
  void OnPairError(ConnectCallback callback, const Error& error);

  void OnCancelPairingError(const Error& error);
  void OnForgetError(ErrorCallback error_callback, const Error& error);

  // Address of this device. Changing this should necessitate creating a new
  // BluetoothDeviceFloss object.
  const std::string address_;

  // Name of this device. Can be queried later and isn't mandatory for creation.
  std::string name_;

  // Whether the device is bonded/paired.
  FlossAdapterClient::BondState bond_state_ =
      FlossAdapterClient::BondState::kNotBonded;

  // Whether the device is connected at link layer level (not profile level).
  // Mirrors the connection state of Floss:
  // 0 if not connected; 1 if connected and > 1 if connection is encrypted
  // (https://android.googlesource.com/platform/system/bt/+/refs/heads/android10-c2f2-release/btif/src/btif_dm.cc#737),
  // but squashing all connected states >= 1 as a single "connected" since it's
  // not used in the Chrome layer.
  bool is_connected_ = false;

  // Represents currently ongoing pairing with this remote device.
  std::unique_ptr<BluetoothPairingFloss> pairing_;

  base::WeakPtrFactory<BluetoothDeviceFloss> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_DEVICE_FLOSS_H_
