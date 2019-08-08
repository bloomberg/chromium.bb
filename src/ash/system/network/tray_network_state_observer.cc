// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/tray_network_state_observer.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

using chromeos::NetworkHandler;

namespace {

const int kUpdateFrequencyMs = 1000;

bool IsWifiEnabled() {
  return NetworkHandler::Get()->network_state_handler()->IsTechnologyEnabled(
      chromeos::NetworkTypePattern::WiFi());
}

bool IsMobileEnabled() {
  return NetworkHandler::Get()->network_state_handler()->IsTechnologyEnabled(
      chromeos::NetworkTypePattern::Cellular() |
      chromeos::NetworkTypePattern::Tether());
}

}  // namespace

namespace ash {

TrayNetworkStateObserver::TrayNetworkStateObserver(Delegate* delegate)
    : delegate_(delegate), update_frequency_(kUpdateFrequencyMs) {
  if (ui::ScopedAnimationDurationScaleMode::duration_scale_mode() !=
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION) {
    update_frequency_ = 0;  // Send updates immediately for tests.
  }
  // TODO(mash): Figure out what to do about NetworkHandler.
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->AddObserver(this,
                                                                FROM_HERE);
    wifi_enabled_ = IsWifiEnabled();
  }
}

TrayNetworkStateObserver::~TrayNetworkStateObserver() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
}

void TrayNetworkStateObserver::NetworkListChanged() {
  SignalUpdate(false /* notify_a11y */);
}

void TrayNetworkStateObserver::DeviceListChanged() {
  SignalUpdate(false /* notify_a11y */);
}

void TrayNetworkStateObserver::ActiveNetworksChanged(
    const std::vector<const chromeos::NetworkState*>& active_networks) {
  SignalUpdate(true /* notify_a11y */);
}

// This tracks Strength and other property changes for all networks. It will
// be called in addition to NetworkConnectionStateChanged for connection state
// changes.
void TrayNetworkStateObserver::NetworkPropertiesUpdated(
    const chromeos::NetworkState* network) {
  SignalUpdate(false /* notify_a11y */);
}

// Required to propagate changes to the "scanning" property of DeviceStates.
void TrayNetworkStateObserver::DevicePropertiesUpdated(
    const chromeos::DeviceState* device) {
  SignalUpdate(false /* notify_a11y */);
}

void TrayNetworkStateObserver::SignalUpdate(bool notify_a11y) {
  bool old_wifi_state = wifi_enabled_;
  wifi_enabled_ = IsWifiEnabled();

  bool old_mobile_state = mobile_enabled_;
  mobile_enabled_ = IsMobileEnabled();

  // Update immediately when Wi-Fi and/or Mobile have been turned on or off.
  // This ensures that the UI for settings and quick settings stays in sync; see
  // https://crbug.com/917325.
  if (old_wifi_state != wifi_enabled_ || old_mobile_state != mobile_enabled_) {
    timer_.Stop();
    SendNetworkStateChanged(notify_a11y);
    return;
  }

  if (timer_.IsRunning())
    return;
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(update_frequency_),
               base::Bind(&TrayNetworkStateObserver::SendNetworkStateChanged,
                          base::Unretained(this), notify_a11y));
}

void TrayNetworkStateObserver::SendNetworkStateChanged(bool notify_a11y) {
  delegate_->NetworkStateChanged(notify_a11y);
}

}  // namespace ash
