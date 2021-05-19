// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/peripheral_battery_listener.h"

#include <string>
#include <vector>

#include "ash/power/hid_battery_util.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

constexpr char kBluetoothDeviceIdPrefix[] = "battery_bluetooth-";

// Currently we expect at most one peripheral charger to exist, and
// it will always be the stylus charger.
constexpr char kStylusChargerFilename[] = "/PCHG0";
constexpr char kStylusChargerID[] = "PCHG0";

// Checks if the device is an external stylus.
bool IsStylusDevice(const std::string& path, const std::string& model_name) {
  std::string identifier = ExtractHIDBatteryIdentifier(path);
  for (const ui::TouchscreenDevice& device :
       ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices()) {
    if (device.has_stylus &&
        (device.name == model_name ||
         device.name.find(model_name) != std::string::npos) &&
        device.sys_path.value().find(identifier) != std::string::npos) {
      return true;
    }
  }

  return false;
}

// Checks if device is the internal charger for an external stylus.
bool IsPeripheralCharger(const std::string& path) {
  return base::EndsWith(path, kStylusChargerFilename);
}

std::string GetMapKeyForBluetoothAddress(const std::string& bluetooth_address) {
  return kBluetoothDeviceIdPrefix + base::ToLowerASCII(bluetooth_address);
}

// Returns the corresponding map key for a HID device.
std::string GetBatteryMapKey(const std::string& path) {
  // Check if the HID path corresponds to a Bluetooth device.
  const std::string bluetooth_address =
      ExtractBluetoothAddressFromHIDBatteryPath(path);
  if (IsPeripheralCharger(path))
    return kStylusChargerID;
  else if (!bluetooth_address.empty())
    return GetMapKeyForBluetoothAddress(bluetooth_address);
  else
    return path;
}

std::string GetBatteryMapKey(device::BluetoothDevice* device) {
  return GetMapKeyForBluetoothAddress(device->GetAddress());
}

PeripheralBatteryListener::BatteryInfo::ChargeStatus
ConvertPowerManagerChargeStatus(
    power_manager::PeripheralBatteryStatus_ChargeStatus incoming) {
  switch (incoming) {
    case power_manager::
        PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_UNKNOWN:
      return PeripheralBatteryListener::BatteryInfo::ChargeStatus::kUnknown;
    case power_manager::
        PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING:
      return PeripheralBatteryListener::BatteryInfo::ChargeStatus::kDischarging;
    case power_manager::
        PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_CHARGING:
      return PeripheralBatteryListener::BatteryInfo::ChargeStatus::kCharging;
    case power_manager::
        PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_NOT_CHARGING:
      return PeripheralBatteryListener::BatteryInfo::ChargeStatus::kNotCharging;
    case power_manager::PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_FULL:
      return PeripheralBatteryListener::BatteryInfo::ChargeStatus::kFull;
    case power_manager::
        PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_ERROR:
      return PeripheralBatteryListener::BatteryInfo::ChargeStatus::kError;
  }
}

}  // namespace

PeripheralBatteryListener::BatteryInfo::BatteryInfo() = default;

PeripheralBatteryListener::BatteryInfo::BatteryInfo(
    const std::string& key,
    const std::u16string& name,
    base::Optional<uint8_t> level,
    base::TimeTicks last_update_timestamp,
    PeripheralType type,
    ChargeStatus charge_status,
    const std::string& bluetooth_address)
    : key(key),
      name(name),
      level(level),
      last_update_timestamp(last_update_timestamp),
      type(type),
      charge_status(charge_status),
      bluetooth_address(bluetooth_address) {}

PeripheralBatteryListener::BatteryInfo::~BatteryInfo() = default;

PeripheralBatteryListener::BatteryInfo::BatteryInfo(const BatteryInfo& info) =
    default;

PeripheralBatteryListener::PeripheralBatteryListener() {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&PeripheralBatteryListener::InitializeOnBluetoothReady,
                     weak_factory_.GetWeakPtr()));
}

PeripheralBatteryListener::~PeripheralBatteryListener() {
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

// Observing chromeos::PowerManagerClient
void PeripheralBatteryListener::PeripheralBatteryStatusReceived(
    const std::string& path,
    const std::string& name,
    int level,
    power_manager::PeripheralBatteryStatus_ChargeStatus pmc_charge_status,
    bool active_update) {
  // Note that zero levels are seen during boot on hid devices; a
  // power_supply node may be created without a real charge level, and
  // we must let it through to allow the BatteryInfo to be created as
  // soon as we are aware of it.
  if (level < -1 || level > 100) {
    LOG(ERROR) << "Invalid battery level " << level << " for device " << name
               << " at path " << path;
    return;
  }

  if (!IsHIDBattery(path) && !IsPeripheralCharger(path)) {
    LOG(ERROR) << "Unsupported battery path " << path;
    return;
  }

  if (!ui::DeviceDataManager::HasInstance() ||
      !ui::DeviceDataManager::GetInstance()->AreDeviceListsComplete()) {
    LOG(ERROR) << "Discarding peripheral battery notification before devices "
                  "are enumerated";
    return;
  }

  BatteryInfo::PeripheralType type;
  if (IsPeripheralCharger(path))
    type = BatteryInfo::PeripheralType::kStylusViaCharger;
  else if (IsStylusDevice(path, name))
    type = BatteryInfo::PeripheralType::kStylusViaScreen;
  else
    type = BatteryInfo::PeripheralType::kOther;

  std::string map_key = GetBatteryMapKey(path);
  base::Optional<uint8_t> opt_level;

  // 0-level charge events can come through when devices are created,
  // usually on boot; for the stylus, convert the level to 'not present',
  // as they are not informative.
  if (level == -1 ||
      (level == 0 &&
       (type == BatteryInfo::PeripheralType::kStylusViaScreen ||
        type == BatteryInfo::PeripheralType::kStylusViaCharger))) {
    opt_level = base::nullopt;
  } else {
    opt_level = level;
  }

  PeripheralBatteryListener::BatteryInfo battery{
      map_key,
      base::ASCIIToUTF16(name),
      opt_level,
      base::TimeTicks::Now(),
      type,
      ConvertPowerManagerChargeStatus(pmc_charge_status),
      ExtractBluetoothAddressFromHIDBatteryPath(path)};

  UpdateBattery(battery, active_update);
}

// Observing device::BluetoothAdapter
void PeripheralBatteryListener::DeviceBatteryChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    base::Optional<uint8_t> new_battery_percentage) {
  if (new_battery_percentage)
    DCHECK_LE(new_battery_percentage.value(), 100);

  BatteryInfo battery{GetBatteryMapKey(device),
                      device->GetNameForDisplay(),
                      new_battery_percentage,
                      base::TimeTicks::Now(),
                      BatteryInfo::PeripheralType::kOther,
                      BatteryInfo::ChargeStatus::kUnknown,
                      device->GetAddress()};

  // Bluetooth does not communicate charge state, do not fill in. Updates
  // will generally pull from the remote device, so consider them active.

  UpdateBattery(battery, /*active_update=*/true);
}

// Observing device::BluetoothAdapter
void PeripheralBatteryListener::DeviceConnectedStateChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool is_now_connected) {
  if (!is_now_connected)
    RemoveBluetoothBattery(device->GetAddress());
}

// Observing device::BluetoothAdapter
void PeripheralBatteryListener::DeviceRemoved(device::BluetoothAdapter* adapter,
                                              device::BluetoothDevice* device) {
  RemoveBluetoothBattery(device->GetAddress());
}

void PeripheralBatteryListener::InitializeOnBluetoothReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  CHECK(bluetooth_adapter_);
  bluetooth_adapter_->AddObserver(this);
}

void PeripheralBatteryListener::RemoveBluetoothBattery(
    const std::string& bluetooth_address) {
  auto it = batteries_.find(kBluetoothDeviceIdPrefix +
                            base::ToLowerASCII(bluetooth_address));
  if (it != batteries_.end()) {
    NotifyRemovingBattery(it->second);
    batteries_.erase(it);
  }
}

void PeripheralBatteryListener::UpdateBattery(const BatteryInfo& battery_info,
                                              bool active_update) {
  const std::string& map_key = battery_info.key;
  auto it = batteries_.find(map_key);

  if (it == batteries_.end()) {
    batteries_[map_key] = battery_info;
    NotifyAddingBattery(batteries_[map_key]);
  } else {
    BatteryInfo& existing_battery_info = it->second;
    // Only some fields should ever change.
    DCHECK(existing_battery_info.bluetooth_address == battery_info.bluetooth_address);
    DCHECK(existing_battery_info.type == battery_info.type);
    existing_battery_info.name = battery_info.name;
    // Ignore a null level for stylus charger updates: we want to memorize
    // the last known actual value. (The touchscreen controller firmware
    // already memorizes this, for that path).
    if (battery_info.type != BatteryInfo::PeripheralType::kStylusViaCharger ||
        battery_info.level)
      existing_battery_info.level = battery_info.level;
    existing_battery_info.last_update_timestamp =
        battery_info.last_update_timestamp;
    existing_battery_info.charge_status = battery_info.charge_status;
  }

  BatteryInfo& info = batteries_[map_key];
  if (active_update) {
    info.last_active_update_timestamp = info.last_update_timestamp;
  }

  NotifyUpdatedBatteryLevel(info);
}

void PeripheralBatteryListener::NotifyAddingBattery(
    const BatteryInfo& battery) {
  for (auto& obs : observers_)
    obs.OnAddingBattery(battery);
}

void PeripheralBatteryListener::NotifyRemovingBattery(
    const BatteryInfo& battery) {
  for (auto& obs : observers_)
    obs.OnRemovingBattery(battery);
}

void PeripheralBatteryListener::NotifyUpdatedBatteryLevel(
    const BatteryInfo& battery) {
  for (auto& obs : observers_)
    obs.OnUpdatedBatteryLevel(battery);
}

void PeripheralBatteryListener::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  // As possible latecomer, introduce observer to batteries that already exist.
  for (auto it : batteries_) {
    observer->OnAddingBattery(it.second);
    observer->OnUpdatedBatteryLevel(it.second);
  }
}

void PeripheralBatteryListener::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PeripheralBatteryListener::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

}  // namespace ash
