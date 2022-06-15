// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/proximity_auth/proximity_auth_system.h"

#include "ash/components/multidevice/logging/logging.h"
#include "ash/components/proximity_auth/proximity_auth_client.h"
#include "ash/components/proximity_auth/remote_device_life_cycle_impl.h"
#include "ash/components/proximity_auth/unlock_manager_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/services/secure_channel/public/cpp/client/secure_channel_client.h"

namespace proximity_auth {

ProximityAuthSystem::ProximityAuthSystem(
    ScreenlockType screenlock_type,
    ProximityAuthClient* proximity_auth_client,
    ash::secure_channel::SecureChannelClient* secure_channel_client)
    : secure_channel_client_(secure_channel_client),
      unlock_manager_(
          std::make_unique<UnlockManagerImpl>(screenlock_type,
                                              proximity_auth_client)) {}

ProximityAuthSystem::ProximityAuthSystem(
    ash::secure_channel::SecureChannelClient* secure_channel_client,
    std::unique_ptr<UnlockManager> unlock_manager)
    : secure_channel_client_(secure_channel_client),
      unlock_manager_(std::move(unlock_manager)) {}

ProximityAuthSystem::~ProximityAuthSystem() {
  ScreenlockBridge::Get()->RemoveObserver(this);
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
}

void ProximityAuthSystem::Start() {
  if (started_)
    return;
  started_ = true;
  ScreenlockBridge::Get()->AddObserver(this);
  const AccountId& focused_account_id =
      ScreenlockBridge::Get()->focused_account_id();
  if (focused_account_id.is_valid())
    OnFocusedUserChanged(focused_account_id);
}

void ProximityAuthSystem::Stop() {
  if (!started_)
    return;
  started_ = false;
  ScreenlockBridge::Get()->RemoveObserver(this);
  OnFocusedUserChanged(EmptyAccountId());
}

void ProximityAuthSystem::SetRemoteDevicesForUser(
    const AccountId& account_id,
    const ash::multidevice::RemoteDeviceRefList& remote_devices,
    absl::optional<ash::multidevice::RemoteDeviceRef> local_device) {
  PA_LOG(VERBOSE) << "Setting devices for user " << account_id.Serialize()
                  << ". Remote device count: " << remote_devices.size()
                  << ", Local device: ["
                  << (local_device.has_value() ? "present" : "absent") << "].";

  remote_devices_map_[account_id] = remote_devices;
  if (local_device) {
    local_device_map_.emplace(account_id, *local_device);
  } else {
    local_device_map_.erase(account_id);
  }

  if (started_) {
    const AccountId& focused_account_id =
        ScreenlockBridge::Get()->focused_account_id();
    if (focused_account_id.is_valid())
      OnFocusedUserChanged(focused_account_id);
  }
}

ash::multidevice::RemoteDeviceRefList
ProximityAuthSystem::GetRemoteDevicesForUser(
    const AccountId& account_id) const {
  if (remote_devices_map_.find(account_id) == remote_devices_map_.end())
    return ash::multidevice::RemoteDeviceRefList();
  return remote_devices_map_.at(account_id);
}

void ProximityAuthSystem::OnAuthAttempted() {
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);
}

void ProximityAuthSystem::OnSuspend() {
  PA_LOG(INFO) << "Preparing for device suspension.";
  DCHECK(!suspended_);
  suspended_ = true;
  OnSuspendOrScreenOffChange();
}

void ProximityAuthSystem::OnSuspendDone() {
  PA_LOG(INFO) << "Device resumed from suspension.";
  DCHECK(suspended_);
  suspended_ = false;
  OnSuspendOrScreenOffChange();
}

void ProximityAuthSystem::OnScreenOff() {
  if (!base::FeatureList::IsEnabled(
          ash::features::kSmartLockBluetoothScreenOffFix)) {
    return;
  }

  PA_LOG(INFO) << "Screen is off.";
  DCHECK(!screen_off_);
  screen_off_ = true;
  OnSuspendOrScreenOffChange();
}

void ProximityAuthSystem::OnScreenOffDone() {
  if (!base::FeatureList::IsEnabled(
          ash::features::kSmartLockBluetoothScreenOffFix)) {
    return;
  }

  // It's possible to end up here when the screen is dimmed and the screen_off_
  // boolean was not true, in which case we can return early.
  if (!screen_off_)
    return;

  PA_LOG(INFO) << "Screen is on.";
  screen_off_ = false;
  OnSuspendOrScreenOffChange();
}

void ProximityAuthSystem::CancelConnectionAttempt() {
  unlock_manager_->CancelConnectionAttempt();
}

std::unique_ptr<RemoteDeviceLifeCycle>
ProximityAuthSystem::CreateRemoteDeviceLifeCycle(
    ash::multidevice::RemoteDeviceRef remote_device,
    absl::optional<ash::multidevice::RemoteDeviceRef> local_device) {
  return std::make_unique<RemoteDeviceLifeCycleImpl>(
      remote_device, local_device, secure_channel_client_);
}

void ProximityAuthSystem::OnScreenDidLock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  const AccountId& focused_account_id =
      ScreenlockBridge::Get()->focused_account_id();
  if (focused_account_id.is_valid())
    OnFocusedUserChanged(focused_account_id);
}

void ProximityAuthSystem::OnScreenDidUnlock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
  remote_device_life_cycle_.reset();
}

void ProximityAuthSystem::OnFocusedUserChanged(const AccountId& account_id) {
  // Update the current RemoteDeviceLifeCycle to the focused user.
  if (remote_device_life_cycle_) {
    if (remote_device_life_cycle_->GetRemoteDevice().user_email() !=
        account_id.GetUserEmail()) {
      PA_LOG(INFO) << "Focused user changed, destroying life cycle for "
                   << account_id.Serialize() << ".";
      unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
      remote_device_life_cycle_.reset();
    } else {
      PA_LOG(INFO) << "Refocused on a user who is already focused.";
      return;
    }
  }

  if (remote_devices_map_.find(account_id) == remote_devices_map_.end() ||
      remote_devices_map_[account_id].size() == 0) {
    PA_LOG(INFO) << "User " << account_id.Serialize()
                 << " does not have a Smart Lock host device.";
    return;
  }
  if (local_device_map_.find(account_id) == local_device_map_.end()) {
    PA_LOG(INFO) << "User " << account_id.Serialize()
                 << " does not have a local device.";
    return;
  }

  // TODO(tengs): We currently assume each user has only one RemoteDevice, so we
  // can simply take the first item in the list.
  ash::multidevice::RemoteDeviceRef remote_device =
      remote_devices_map_[account_id][0];

  absl::optional<ash::multidevice::RemoteDeviceRef> local_device;
  local_device = local_device_map_.at(account_id);

  if (!suspended_ && !screen_off_) {
    PA_LOG(INFO) << "Creating RemoteDeviceLifeCycle for focused user: "
                 << account_id.Serialize();
    remote_device_life_cycle_ =
        CreateRemoteDeviceLifeCycle(remote_device, local_device);

    // UnlockManager listens for Bluetooth power change events, and is therefore
    // responsible for starting RemoteDeviceLifeCycle when Bluetooth becomes
    // powered.
    unlock_manager_->SetRemoteDeviceLifeCycle(remote_device_life_cycle_.get());
  }
}

std::string ProximityAuthSystem::GetLastRemoteStatusUnlockForLogging() {
  if (unlock_manager_) {
    return unlock_manager_->GetLastRemoteStatusUnlockForLogging();
  }
  return std::string();
}

void ProximityAuthSystem::OnSuspendOrScreenOffChange() {
  if (suspended_ || screen_off_) {
    unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
    remote_device_life_cycle_.reset();
    return;
  }

  if (!ScreenlockBridge::Get()->IsLocked()) {
    PA_LOG(INFO) << "System resumed, but no lock screen.";
  } else if (!started_) {
    PA_LOG(INFO) << "System resumed, but ProximityAuthSystem is stopped.";
  } else {
    OnFocusedUserChanged(ScreenlockBridge::Get()->focused_account_id());
  }
}

}  // namespace proximity_auth
