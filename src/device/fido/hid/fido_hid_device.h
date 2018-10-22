// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_HID_FIDO_HID_DEVICE_H_
#define DEVICE_FIDO_HID_FIDO_HID_DEVICE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/apdu/apdu_command.h"
#include "components/apdu/apdu_response.h"
#include "device/fido/fido_device.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

class FidoHidMessage;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoHidDevice : public FidoDevice {
 public:
  // HID transport layer error constants that are returned to the client.
  // Carried in the payload section of the Error command.
  // https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#ctaphid-commands
  enum class HidErrorConstant : uint8_t {
    kInvalidCommand = 0x01,
    kInvalidParameter = 0x02,
    kInvalidLength = 0x03,
    kInvalidSequence = 0x04,
    kTimeout = 0x05,
    kBusy = 0x06,
    kLockRequired = 0x0a,
    kInvalidChannel = 0x0b,
    kOther = 0x7f,
  };

  FidoHidDevice(device::mojom::HidDeviceInfoPtr device_info,
                device::mojom::HidManager* hid_manager);
  ~FidoHidDevice() final;

  // Send a command to this device.
  void DeviceTransact(std::vector<uint8_t> command,
                      DeviceCallback callback) final;

  // FidoDevice:
  // Send a wink command if supported.
  void TryWink(WinkCallback callback) final;
  // Send command to cancel any outstanding requests on this device.
  void Cancel() final;
  // Use a string identifier to compare to other devices.
  std::string GetId() const final;
  FidoTransportProtocol DeviceTransport() const final;

  // Get a string identifier for a given device info.
  static std::string GetIdForDevice(
      const device::mojom::HidDeviceInfo& device_info);

 private:
  FRIEND_TEST_ALL_PREFIXES(FidoHidDeviceTest, TestConnectionFailure);
  FRIEND_TEST_ALL_PREFIXES(FidoHidDeviceTest, TestDeviceError);
  FRIEND_TEST_ALL_PREFIXES(FidoHidDeviceTest, TestRetryChannelAllocation);
  FRIEND_TEST_ALL_PREFIXES(FidoHidDeviceTest, TestCancel);

  static constexpr uint8_t kWinkCapability = 0x01;
  static constexpr uint8_t kLockCapability = 0x02;
  static constexpr uint32_t kBroadcastChannel = 0xffffffff;

  using HidMessageCallback =
      base::OnceCallback<void(base::Optional<FidoHidMessage>)>;
  using ConnectCallback = device::mojom::HidManager::ConnectCallback;

  // Open a connection to this device.
  void Connect(ConnectCallback callback);
  void OnConnect(std::vector<uint8_t> command,
                 DeviceCallback callback,
                 device::mojom::HidConnectionPtr connection);
  // Ask device to allocate a unique channel id for this connection.
  void AllocateChannel(std::vector<uint8_t> command, DeviceCallback callback);
  void OnAllocateChannel(std::vector<uint8_t> nonce,
                         std::vector<uint8_t> command,
                         DeviceCallback callback,
                         base::Optional<FidoHidMessage> message);
  void Transition(std::vector<uint8_t> command, DeviceCallback callback);
  // Write all message packets to device, and read response if expected.
  void WriteMessage(base::Optional<FidoHidMessage> message,
                    bool response_expected,
                    HidMessageCallback callback);
  void PacketWritten(base::Optional<FidoHidMessage> message,
                     bool response_expected,
                     HidMessageCallback callback,
                     bool success);
  // Read all response message packets from device.
  void ReadMessage(HidMessageCallback callback);
  void MessageReceived(DeviceCallback callback,
                       base::Optional<FidoHidMessage> message);
  void OnRead(HidMessageCallback callback,
              bool success,
              uint8_t report_id,
              const base::Optional<std::vector<uint8_t>>& buf);
  void OnReadContinuation(base::Optional<FidoHidMessage> message,
                          HidMessageCallback callback,
                          bool success,
                          uint8_t report_id,
                          const base::Optional<std::vector<uint8_t>>& buf);
  void OnKeepAlive(DeviceCallback callback);
  void OnWink(WinkCallback callback, base::Optional<FidoHidMessage> response);
  void ArmTimeout(DeviceCallback callback);
  void OnTimeout(DeviceCallback callback);
  void ProcessHidError(FidoHidDeviceCommand cmd,
                       base::span<const uint8_t> payload);

  base::WeakPtr<FidoDevice> GetWeakPtr() override;

  uint32_t channel_id_ = kBroadcastChannel;
  uint8_t capabilities_ = 0;

  base::CancelableOnceClosure timeout_callback_;
  base::queue<std::pair<std::vector<uint8_t>, DeviceCallback>>
      pending_transactions_;

  // All the FidoHidDevice instances are owned by U2fRequest. So it is safe to
  // let the FidoHidDevice share the device::mojo::HidManager raw pointer from
  // U2fRequest.
  device::mojom::HidManager* hid_manager_;
  device::mojom::HidDeviceInfoPtr device_info_;
  device::mojom::HidConnectionPtr connection_;
  base::WeakPtrFactory<FidoHidDevice> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FidoHidDevice);
};

}  // namespace device

#endif  // DEVICE_FIDO_HID_FIDO_HID_DEVICE_H_
