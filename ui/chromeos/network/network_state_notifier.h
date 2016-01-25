// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_NETWORK_NETWORK_STATE_NOTIFIER_H_
#define UI_CHROMEOS_NETWORK_NETWORK_STATE_NOTIFIER_H_

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/network/network_connection_observer.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "ui/chromeos/ui_chromeos_export.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {
class NetworkState;
}

namespace ui {

class NetworkConnect;

// This class provides user notifications in the following cases:
// 1. ShowNetworkConnectError() gets called after any user initiated connect
//    failure. This will handle displaying an error notification.
//    TODO(stevenjb): convert this class to use the new MessageCenter
//    notification system.
// 2. It observes NetworkState changes to generate notifications when a
//    Cellular network is out of credits.
// 3. Generates a notification when VPN is disconnected not as a result of
//    user's action.
class UI_CHROMEOS_EXPORT NetworkStateNotifier
    : public chromeos::NetworkConnectionObserver,
      public chromeos::NetworkStateHandlerObserver {
 public:
  explicit NetworkStateNotifier(NetworkConnect* network_connect);
  ~NetworkStateNotifier() override;

  // NetworkConnectionObserver
  void ConnectToNetworkRequested(const std::string& service_path) override;
  void ConnectSucceeded(const std::string& service_path) override;
  void ConnectFailed(const std::string& service_path,
                     const std::string& error_name) override;
  void DisconnectRequested(const std::string& service_path) override;

  // NetworkStateHandlerObserver
  void DefaultNetworkChanged(const chromeos::NetworkState* network) override;
  void NetworkConnectionStateChanged(
      const chromeos::NetworkState* network) override;
  void NetworkPropertiesUpdated(const chromeos::NetworkState* network) override;

  // Show a connection error notification. If |error_name| matches an error
  // defined in NetworkConnectionHandler for connect, configure, or activation
  // failed, then the associated message is shown; otherwise use the last_error
  // value for the network or a Shill property if available.
  void ShowNetworkConnectError(const std::string& error_name,
                               const std::string& service_path);

  // Show a mobile activation error notification.
  void ShowMobileActivationError(const std::string& service_path);

  static const char kNotifierNetwork[];
  static const char kNotifierNetworkError[];
  static const char kNetworkConnectNotificationId[];
  static const char kNetworkActivateNotificationId[];
  static const char kNetworkOutOfCreditsNotificationId[];

 private:
  void ConnectErrorPropertiesSucceeded(
      const std::string& error_name,
      const std::string& service_path,
      const base::DictionaryValue& shill_properties);
  void ConnectErrorPropertiesFailed(
      const std::string& error_name,
      const std::string& service_path,
      const std::string& shill_connect_error,
      scoped_ptr<base::DictionaryValue> shill_error_data);
  void ShowConnectErrorNotification(
      const std::string& error_name,
      const std::string& service_path,
      const base::DictionaryValue& shill_properties);
  void ShowVpnDisconnectedNotification(const chromeos::NetworkState* vpn);

  // Removes any existing connect notifications.
  void RemoveConnectNotification();

  // Returns true if the default network changed.
  bool UpdateDefaultNetwork(const chromeos::NetworkState* network);

  // Helper methods to update state and check for notifications.
  void UpdateVpnConnectionState(const chromeos::NetworkState* vpn);
  void UpdateCellularOutOfCredits(const chromeos::NetworkState* cellular);
  void UpdateCellularActivating(const chromeos::NetworkState* cellular);

  // Invokes network_connect_->ShowNetworkSettingsForPath from a callback.
  void ShowNetworkSettingsForPath(const std::string& service_path);

  NetworkConnect* network_connect_;  // unowned
  std::string last_default_network_;
  bool did_show_out_of_credits_;
  base::Time out_of_credits_notify_time_;
  std::set<std::string> cellular_activating_;
  std::string connected_vpn_;
  base::WeakPtrFactory<NetworkStateNotifier> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStateNotifier);
};

}  // namespace ui

#endif  // UI_CHROMEOS_NETWORK_NETWORK_STATE_NOTIFIER_H_
