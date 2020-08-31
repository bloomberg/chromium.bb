// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/tray_bluetooth_helper_legacy.h"

#include <string>
#include <utility>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/system/bluetooth/bluetooth_power_controller.h"
#include "ash/system/model/system_tray_model.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"

using device::mojom::BluetoothDeviceBatteryInfo;
using device::mojom::BluetoothDeviceBatteryInfoPtr;
using device::mojom::BluetoothDeviceInfo;
using device::mojom::BluetoothDeviceInfoPtr;
using device::mojom::BluetoothSystem;

namespace ash {
namespace {

// System tray shows a limited number of bluetooth devices.
const int kMaximumDevicesShown = 50;

device::ConnectionFailureReason GetConnectionFailureReason(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_FAILED:
      return device::ConnectionFailureReason::kAuthFailed;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_TIMEOUT:
      return device::ConnectionFailureReason::kAuthTimeout;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_FAILED:
      return device::ConnectionFailureReason::kFailed;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN:
      return device::ConnectionFailureReason::kUnknownConnectionError;
    case device::BluetoothDevice::ConnectErrorCode::ERROR_UNSUPPORTED_DEVICE:
      return device::ConnectionFailureReason::kUnsupportedDevice;
    default:
      return device::ConnectionFailureReason::kUnknownError;
  }
}

void BluetoothSetDiscoveringError() {
  LOG(ERROR) << "BluetoothSetDiscovering failed.";
}

void OnBluetoothDeviceConnect(bool was_device_already_paired) {
  if (was_device_already_paired) {
    device::RecordUserInitiatedReconnectionAttemptResult(
        base::nullopt /* failure_reason */,
        device::BluetoothUiSurface::kSystemTray);
  }
}

void OnBluetoothDeviceConnectError(
    bool was_device_already_paired,
    device::BluetoothDevice::ConnectErrorCode error_code) {
  LOG(ERROR) << "Failed to connect to device, error code [" << error_code
             << "]. The attempted device was previously ["
             << (was_device_already_paired ? "paired" : "not paired") << "].";

  if (was_device_already_paired) {
    device::RecordUserInitiatedReconnectionAttemptResult(
        GetConnectionFailureReason(error_code),
        device::BluetoothUiSurface::kSystemTray);
  }
}

std::string BluetoothAddressToStr(const BluetoothAddress& address) {
  static constexpr char kAddressFormat[] =
      "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX";
  return base::StringPrintf(kAddressFormat, address[0], address[1], address[2],
                            address[3], address[4], address[5]);
}

// Converts a MAC Address string e.g. "00:11:22:33:44:55" into an
// BluetoothAddress e.g. {0x00, 0x11, 0x22, 0x33, 0x44, 0x55}.
BluetoothAddress AddressStrToBluetoothAddress(const std::string& address_str) {
  BluetoothAddress address_array;

  // If the string is not a valid encoding of a Bluetooth address, then the
  // underlying Bluetooth API returned an incorrect value.
  CHECK(device::BluetoothDevice::ParseAddress(address_str, address_array));

  return address_array;
}

BluetoothDeviceInfoPtr GetBluetoothDeviceInfo(device::BluetoothDevice* device) {
  BluetoothDeviceInfoPtr info = BluetoothDeviceInfo::New();
  info->address = AddressStrToBluetoothAddress(device->GetAddress());
  info->name = device->GetName();
  info->is_paired = device->IsPaired();
  if (device->battery_percentage()) {
    info->battery_info =
        BluetoothDeviceBatteryInfo::New(device->battery_percentage().value());
  }

  switch (device->GetDeviceType()) {
    case device::BluetoothDeviceType::UNKNOWN:
      info->device_type = BluetoothDeviceInfo::DeviceType::kUnknown;
      break;
    case device::BluetoothDeviceType::COMPUTER:
      info->device_type = BluetoothDeviceInfo::DeviceType::kComputer;
      break;
    case device::BluetoothDeviceType::PHONE:
      info->device_type = BluetoothDeviceInfo::DeviceType::kPhone;
      break;
    case device::BluetoothDeviceType::MODEM:
      info->device_type = BluetoothDeviceInfo::DeviceType::kModem;
      break;
    case device::BluetoothDeviceType::AUDIO:
      info->device_type = BluetoothDeviceInfo::DeviceType::kAudio;
      break;
    case device::BluetoothDeviceType::CAR_AUDIO:
      info->device_type = BluetoothDeviceInfo::DeviceType::kCarAudio;
      break;
    case device::BluetoothDeviceType::VIDEO:
      info->device_type = BluetoothDeviceInfo::DeviceType::kVideo;
      break;
    case device::BluetoothDeviceType::PERIPHERAL:
      info->device_type = BluetoothDeviceInfo::DeviceType::kPeripheral;
      break;
    case device::BluetoothDeviceType::JOYSTICK:
      info->device_type = BluetoothDeviceInfo::DeviceType::kJoystick;
      break;
    case device::BluetoothDeviceType::GAMEPAD:
      info->device_type = BluetoothDeviceInfo::DeviceType::kGamepad;
      break;
    case device::BluetoothDeviceType::KEYBOARD:
      info->device_type = BluetoothDeviceInfo::DeviceType::kKeyboard;
      break;
    case device::BluetoothDeviceType::MOUSE:
      info->device_type = BluetoothDeviceInfo::DeviceType::kMouse;
      break;
    case device::BluetoothDeviceType::TABLET:
      info->device_type = BluetoothDeviceInfo::DeviceType::kTablet;
      break;
    case device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO:
      info->device_type = BluetoothDeviceInfo::DeviceType::kKeyboardMouseCombo;
      break;
  }

  if (device->IsConnecting()) {
    info->connection_state = BluetoothDeviceInfo::ConnectionState::kConnecting;
  } else if (device->IsConnected()) {
    info->connection_state = BluetoothDeviceInfo::ConnectionState::kConnected;
  } else {
    info->connection_state =
        BluetoothDeviceInfo::ConnectionState::kNotConnected;
  }

  return info;
}

}  // namespace

TrayBluetoothHelperLegacy::TrayBluetoothHelperLegacy() {}

TrayBluetoothHelperLegacy::~TrayBluetoothHelperLegacy() {
  if (adapter_)
    adapter_->RemoveObserver(this);
}

void TrayBluetoothHelperLegacy::InitializeOnAdapterReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  CHECK(adapter_);
  adapter_->AddObserver(this);

  last_state_ = GetBluetoothState();
  StartOrStopRefreshingDeviceList();
}

void TrayBluetoothHelperLegacy::Initialize() {
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&TrayBluetoothHelperLegacy::InitializeOnAdapterReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrayBluetoothHelperLegacy::StartBluetoothDiscovering() {
  discovery_start_timestamp_ = base::DefaultClock::GetInstance()->Now();

  if (HasBluetoothDiscoverySession()) {
    LOG(WARNING) << "Already have active Bluetooth device discovery session.";
    return;
  }
  VLOG(1) << "Requesting new Bluetooth device discovery session.";
  should_run_discovery_ = true;
  adapter_->StartDiscoverySession(
      base::BindOnce(&TrayBluetoothHelperLegacy::OnStartDiscoverySession,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BluetoothSetDiscoveringError));
}

void TrayBluetoothHelperLegacy::StopBluetoothDiscovering() {
  discovery_start_timestamp_ = base::Time();

  should_run_discovery_ = false;
  if (!HasBluetoothDiscoverySession()) {
    LOG(WARNING) << "No active Bluetooth device discovery session.";
    return;
  }
  VLOG(1) << "Stopping Bluetooth device discovery session.";
  discovery_session_.reset();
}

void TrayBluetoothHelperLegacy::ConnectToBluetoothDevice(
    const BluetoothAddress& address) {
  device::BluetoothDevice* device =
      adapter_->GetDevice(BluetoothAddressToStr(address));
  if (!device || device->IsConnecting() ||
      (device->IsConnected() && device->IsPaired())) {
    return;
  }

  if (!discovery_start_timestamp_.is_null()) {
    device::RecordDeviceSelectionDuration(
        base::DefaultClock::GetInstance()->Now() - discovery_start_timestamp_,
        device::BluetoothUiSurface::kSystemTray, device->IsPaired(),
        device->GetType());
    discovery_start_timestamp_ = base::Time();
  }

  // Extra consideration taken for already paired devices, for metrics
  // collection.
  if (device->IsPaired()) {
    base::RecordAction(
        base::UserMetricsAction("StatusArea_Bluetooth_Connect_Known"));

    if (!device->IsConnectable()) {
      device::RecordUserInitiatedReconnectionAttemptResult(
          device::ConnectionFailureReason::kNotConnectable,
          device::BluetoothUiSurface::kSystemTray);
      return;
    }

    device->Connect(nullptr /* pairing_delegate */,
                    base::BindOnce(&OnBluetoothDeviceConnect,
                                   true /* was_device_already_paired */),
                    base::BindOnce(&OnBluetoothDeviceConnectError,
                                   true /* was_device_already_paired */));
    return;
  }

  // Simply connect without pairing for devices which do not support pairing.
  if (!device->IsPairable()) {
    device->Connect(nullptr /* pairing_delegate */, base::DoNothing(),
                    base::BindOnce(&OnBluetoothDeviceConnectError,
                                   false /* was_device_already_paired */));
    return;
  }

  // Show pairing dialog for the unpaired device; this kicks off pairing.
  Shell::Get()->system_tray_model()->client()->ShowBluetoothPairingDialog(
      device->GetAddress(), device->GetNameForDisplay(), device->IsPaired(),
      device->IsConnected());
}

BluetoothSystem::State TrayBluetoothHelperLegacy::GetBluetoothState() {
  // Eventually this will use the BluetoothSystem Mojo interface, but for now
  // use the current Bluetooth API to get a BluetoothSystem::State.
  if (!adapter_)
    return BluetoothSystem::State::kUnavailable;
  if (!adapter_->IsPresent())
    return BluetoothSystem::State::kUnavailable;
  if (adapter_->IsPowered())
    return BluetoothSystem::State::kPoweredOn;

  return BluetoothSystem::State::kPoweredOff;
}

void TrayBluetoothHelperLegacy::SetBluetoothEnabled(bool enabled) {
  if (enabled != (GetBluetoothState() == BluetoothSystem::State::kPoweredOn)) {
    Shell::Get()->metrics()->RecordUserMetricsAction(
        enabled ? UMA_STATUS_AREA_BLUETOOTH_ENABLED
                : UMA_STATUS_AREA_BLUETOOTH_DISABLED);
  }

  Shell::Get()->bluetooth_power_controller()->SetBluetoothEnabled(enabled);
}

bool TrayBluetoothHelperLegacy::HasBluetoothDiscoverySession() {
  return discovery_session_ && discovery_session_->IsActive();
}

void TrayBluetoothHelperLegacy::GetBluetoothDevices(
    GetBluetoothDevicesCallback callback) const {
  BluetoothDeviceList device_list;
  device::BluetoothAdapter::DeviceList devices =
      device::FilterBluetoothDeviceList(adapter_->GetDevices(),
                                        device::BluetoothFilterType::KNOWN,
                                        kMaximumDevicesShown);
  for (device::BluetoothDevice* device : devices)
    device_list.push_back(GetBluetoothDeviceInfo(device));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(device_list)));
}

////////////////////////////////////////////////////////////////////////////////
// BluetoothAdapter::Observer:

void TrayBluetoothHelperLegacy::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  if (last_state_ == GetBluetoothState())
    return;

  last_state_ = GetBluetoothState();
  NotifyBluetoothSystemStateChanged();
  StartOrStopRefreshingDeviceList();
}

void TrayBluetoothHelperLegacy::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  if (last_state_ == GetBluetoothState())
    return;

  last_state_ = GetBluetoothState();
  NotifyBluetoothSystemStateChanged();
  StartOrStopRefreshingDeviceList();
}

void TrayBluetoothHelperLegacy::AdapterDiscoveringChanged(
    device::BluetoothAdapter* adapter,
    bool discovering) {
  NotifyBluetoothScanStateChanged();
}

void TrayBluetoothHelperLegacy::OnStartDiscoverySession(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  // If the discovery session was returned after a request to stop discovery
  // (e.g. the user dismissed the Bluetooth detailed view before the call
  // returned), don't claim the discovery session and let it clean up.
  if (!should_run_discovery_)
    return;
  VLOG(1) << "Claiming new Bluetooth device discovery session.";
  discovery_session_ = std::move(discovery_session);
  NotifyBluetoothScanStateChanged();
}

}  // namespace ash
