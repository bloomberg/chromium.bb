// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WIN_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"

namespace device {

class BluetoothAdapterWin;
class BluetoothRemoteGattServiceWin;
class BluetoothServiceRecordWin;
class BluetoothSocketThread;
class BluetoothUUID;

class DEVICE_BLUETOOTH_EXPORT BluetoothDeviceWin
    : public BluetoothDevice,
      public BluetoothAdapter::Observer {
 public:
  explicit BluetoothDeviceWin(
      BluetoothAdapterWin* adapter,
      const BluetoothTaskManagerWin::DeviceState& device_state,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<BluetoothSocketThread> socket_thread);
  ~BluetoothDeviceWin() override;

  // BluetoothDevice override
  uint32_t GetBluetoothClass() const override;
  std::string GetAddress() const override;
  VendorIDSource GetVendorIDSource() const override;
  uint16_t GetVendorID() const override;
  uint16_t GetProductID() const override;
  uint16_t GetDeviceID() const override;
  uint16_t GetAppearance() const override;
  base::Optional<std::string> GetName() const override;
  bool IsPaired() const override;
  bool IsConnected() const override;
  bool IsGattConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
  UUIDSet GetUUIDs() const override;
  base::Optional<int8_t> GetInquiryRSSI() const override;
  base::Optional<int8_t> GetInquiryTxPower() const override;
  bool ExpectingPinCode() const override;
  bool ExpectingPasskey() const override;
  bool ExpectingConfirmation() const override;
  void GetConnectionInfo(const ConnectionInfoCallback& callback) override;
  void SetConnectionLatency(ConnectionLatency connection_latency,
                            const base::Closure& callback,
                            const ErrorCallback& error_callback) override;
  void Connect(PairingDelegate* pairing_delegate,
               base::OnceClosure callback,
               ConnectErrorCallback error_callback) override;
  void SetPinCode(const std::string& pincode) override;
  void SetPasskey(uint32_t passkey) override;
  void ConfirmPairing() override;
  void RejectPairing() override;
  void CancelPairing() override;
  void Disconnect(const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  void Forget(const base::Closure& callback,
              const ErrorCallback& error_callback) override;
  void ConnectToService(
      const BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;
  void ConnectToServiceInsecurely(
      const BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;

  // Used by BluetoothProfileWin to retrieve the service record for the given
  // |uuid|.
  const BluetoothServiceRecordWin* GetServiceRecord(
      const device::BluetoothUUID& uuid) const;

  // Returns true if all fields and services of this instance are equal to the
  // fields and services stored in |device_state|.
  bool IsEqual(const BluetoothTaskManagerWin::DeviceState& device_state);

  // Updates this instance with all fields and properties stored in
  // |device_state|.
  void Update(const BluetoothTaskManagerWin::DeviceState& device_state);

  // Notify |service| discovery complete, |service| is a remote GATT service of
  // this device.
  void GattServiceDiscoveryComplete(BluetoothRemoteGattServiceWin* service);

 protected:
  // BluetoothDevice override
  void CreateGattConnectionImpl(
      base::Optional<BluetoothUUID> service_uuid) override;
  void DisconnectGatt() override;

 private:
  friend class BluetoothAdapterWin;

  // Used by BluetoothAdapterWin to update the visible state during
  // discovery.
  void SetVisible(bool visible);

  // Updates the services with services stored in |device_state|.
  void UpdateServices(const BluetoothTaskManagerWin::DeviceState& device_state);

  // Checks if GATT service with |uuid| and |attribute_handle| has already been
  // discovered.
  bool IsGattServiceDiscovered(const BluetoothUUID& uuid,
                               uint16_t attribute_handle);

  // Checks if |service| still exist on device according to newly discovered
  // |service_state|.
  bool DoesGattServiceExist(
      const std::vector<std::unique_ptr<
          BluetoothTaskManagerWin::ServiceRecordState>>& service_state,
      BluetoothRemoteGattService* service);

  // Updates the GATT services with the services stored in |service_state|.
  void UpdateGattServices(
      const std::vector<
          std::unique_ptr<BluetoothTaskManagerWin::ServiceRecordState>>&
          service_state);

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<BluetoothSocketThread> socket_thread_;

  // The Bluetooth class of the device, a bitmask that may be decoded using
  // https://www.bluetooth.org/Technical/AssignedNumbers/baseband.htm
  uint32_t bluetooth_class_;

  // The name of the device, as supplied by the remote device.
  base::Optional<std::string> name_;

  // The Bluetooth address of the device.
  std::string address_;

  // Tracked device state, updated by the adapter managing the lifecycle of
  // the device.
  bool paired_;
  bool connected_;
  bool is_low_energy_;

  // Used to send change notifications when a device disappears during
  // discovery.
  bool visible_;

  // The services (identified by UUIDs) that this device provides.
  UUIDSet uuids_;

  // The service records retrieved from SDP.
  std::vector<std::unique_ptr<BluetoothServiceRecordWin>> service_record_list_;

  // The element of the set is the uuid / attribute handle pair of the
  // BluetoothRemoteGattServiceWin instance.
  std::set<std::pair<BluetoothUUID, uint16_t>>
      discovery_completed_included_services_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WIN_H_
