// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_AUTO_CONNECT_HANDLER_H_
#define CHROMEOS_NETWORK_AUTO_CONNECT_HANDLER_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/client_cert_resolver.h"
#include "chromeos/network/network_connection_observer.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_policy_observer.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) AutoConnectHandler
    : public LoginState::Observer,
      public NetworkPolicyObserver,
      public NetworkConnectionObserver,
      public NetworkStateHandlerObserver,
      public ClientCertResolver::Observer {
 public:
  enum AutoConnectReason {
    AUTO_CONNECT_REASON_LOGGED_IN = 1,
    AUTO_CONNECT_REASON_POLICY_APPLIED = 1 << 1,
    AUTO_CONNECT_REASON_CERTIFICATE_RESOLVED = 1 << 2
  };

  class Observer {
   public:
    // Note: |auto_connect_reasons| is computed by applying the bitwise OR
    // operation to all AutoConnectReasons which triggered auto-connect.
    virtual void OnAutoConnectedInitiated(int auto_connect_reasons) = 0;
  };

  AutoConnectHandler(const AutoConnectHandler&) = delete;
  AutoConnectHandler& operator=(const AutoConnectHandler&) = delete;

  ~AutoConnectHandler() override;

  // LoginState::Observer
  void LoggedInStateChanged() override;

  // NetworkConnectionObserver
  void ConnectToNetworkRequested(const std::string& service_path) override;

  // NetworkPolicyObserver
  void PoliciesApplied(const std::string& userhash) override;

  // NetworkStateHandlerObserver
  void ScanStarted(const DeviceState* device) override;
  void ScanCompleted(const DeviceState* device) override;

  // ClientCertResolver::Observer
  void ResolveRequestCompleted(bool network_properties_changed) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyAutoConnectInitiatedForTest(int auto_connect_reasons);

 private:
  friend class NetworkHandler;
  friend class AutoConnectHandlerTest;

  AutoConnectHandler();

  void Init(ClientCertResolver* client_cert_resolver,
            NetworkConnectionHandler* network_connection_handler,
            NetworkStateHandler* network_state_handler,
            ManagedNetworkConfigurationHandler*
                managed_network_configuration_handler);

  void NotifyAutoConnectInitiated(int auto_connect_reasons);

  // This function is called whenever the logged in state changes or when a new
  // WiFi policy is applied. Once both device and user policy have been applied
  // and either of AllowOnlyPolicyWiFiNetworksToConnect,
  // AllowOnlyPolicyWiFiNetworksToConnectIfAvailable or
  // AllowOnlyPolicyNetworksToAutoconnect is enabled, we disconnect from all
  // connecting/connected unmanaged WiFi networks and either remove the network
  // configuration (for AllowOnlyPolicyWiFiNetworksToConnect*) or only disable
  // auto-connect (for AllowOnlyPolicyNetworksToAutoconnect) for all unmanaged
  // WiFi networks (see |DisconnectAndRemoveConfigOrDisableAutoConnect(...)|).
  // For the AllowOnlyPolicyNetworksToAutoconnect policy we only disconnect once
  // to allow managed networks to auto-connect and prevent disconnects with
  // manually connected unmanaged WiFi networks on every policy update.
  void DisconnectWiFiIfPolicyRequires();

  // This function is similar to |DisconnectWifiIfPolicyRequires| but handles
  // disconnects for cellular networks. Disconnect occurs once device or user
  // policy have been applied and AllowOnlyPolicyCellularNetworks or
  // AllowOnlyPolicyNetworksToAutoconnect is enabled.
  void DisconnectCellularIfPolicyRequires();

  // Disconnects the unmanaged network in the given |networks| list and removes
  // the network configuration if it's either a Cellular type network or
  // |available_only| is not set for WiFi type network. If
  // |only_managed_autoconnect| is set to true, it also disables auto connect
  // for the unmanaged network configuration.
  void DisconnectAndRemoveConfigOrDisableAutoConnect(
      const NetworkStateHandler::NetworkStateList& networks,
      bool only_managed_autoconnect,
      bool available_only);

  // Disconnects the connection to the |network|.
  void DisconnectNetwork(const NetworkState* network);

  // Removes the network configuration for the network represented by
  // |service_path|.
  void RemoveNetworkConfigurationForNetwork(const std::string& service_path);

  // Sets AutoConnect=false for the |network_type| network represented by
  // |service_path|.
  void DisableAutoconnectForNetwork(const std::string& service_path,
                                    const std::string& network_type);

  // Requests and if possible connects to the 'best' available network, see
  // CheckBestConnection().
  void RequestBestConnection(AutoConnectReason auto_connect_reason);

  // If a request to connect to the best network is pending and all requirements
  // are fulfilled (like policy loaded, certificate patterns being resolved),
  // then this will call ConnectToBestWifiNetwork of |network_state_handler_|.
  void CheckBestConnection();

  // Calls Shill.Manager.ConnectToBestServices().
  void CallShillConnectToBestServices();

  // Returns all hidden hex SSIDs that are currently configured in shill.
  std::set<std::string> GetConfiguredHiddenHexSsids();

  // Local references to the associated handler instances.
  ClientCertResolver* client_cert_resolver_;
  NetworkConnectionHandler* network_connection_handler_;
  NetworkStateHandler* network_state_handler_;
  ManagedNetworkConfigurationHandler* managed_configuration_handler_;

  // Whether a request to connect to the best network is pending. If true, once
  // all requirements are met (like policy loaded, certificate patterns being
  // resolved), a scan will be requested and ConnectToBestServices will be
  // triggered once it completes.
  bool request_best_connection_pending_;

  // Whether the device policy, which might be empty, is already applied.
  bool device_policy_applied_;

  // Whether the user policy of the first user who logged in is already applied.
  // The policy might be empty.
  bool user_policy_applied_;

  // Whether at least once client certificate patterns were checked and if any
  // existed resolved. Even if there are no certificate patterns, this will be
  // eventually true.
  bool client_certs_resolved_;

  // Whether the autoconnect policy was applied already, see
  // DisconnectWiFiIfPolicyRequires() and DisconnectCellularIfPolicyRequires().
  bool applied_autoconnect_policy_on_wifi;
  bool applied_autoconnect_policy_on_cellular;

  // When true, trigger ConnectToBestServices after the next scan completion.
  bool connect_to_best_services_after_scan_;

  // The bitwise OR of all AutoConnectReason which have triggered auto-
  // connection.
  int auto_connect_reasons_;

  // Set of hex SSIDs that were configured as hidden SSIDs when the current scan
  // started. Empty if no (known) scan is in progress.
  std::set<std::string> hidden_hex_ssids_at_scan_start_;

  // When true, a scan has been re-requested because the set of hidden SSIDs
  // changed during a scan and a ConnectToBestServices call is pending.
  bool rescan_triggered_due_to_hidden_ssids_ = false;

  base::ObserverList<Observer>::Unchecked observer_list_;

  base::WeakPtrFactory<AutoConnectHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_AUTO_CONNECT_HANDLER_H_
