// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/network/network_state_notifier.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/shill_property_util.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/network/network_connect.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"

using chromeos::NetworkConnectionHandler;
using chromeos::NetworkHandler;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace {

const int kMinTimeBetweenOutOfCreditsNotifySeconds = 10 * 60;

// Ignore in-progress error.
bool ShillErrorIsIgnored(const std::string& shill_error) {
  if (shill_error == shill::kErrorResultInProgress)
    return true;
  return false;
}

// Error messages based on |error_name|, not network_state->error().
base::string16 GetConnectErrorString(const std::string& error_name) {
  if (error_name == NetworkConnectionHandler::kErrorNotFound)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_CONNECT_FAILED);
  if (error_name == NetworkConnectionHandler::kErrorConfigureFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_CONFIGURE_FAILED);
  }
  if (error_name == NetworkConnectionHandler::kErrorCertLoadTimeout) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_CERTIFICATES_NOT_LOADED);
  }
  if (error_name == ui::NetworkConnect::kErrorActivateFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_ACTIVATION_FAILED);
  }
  return base::string16();
}

int GetErrorNotificationIconId(const std::string& network_type) {
  if (network_type == shill::kTypeVPN)
    return IDR_AURA_UBER_TRAY_NETWORK_VPN;
  if (network_type == shill::kTypeCellular)
    return IDR_AURA_UBER_TRAY_NETWORK_FAILED_CELLULAR;
  return IDR_AURA_UBER_TRAY_NETWORK_FAILED;
}

void ShowErrorNotification(const std::string& service_path,
                           const std::string& notification_id,
                           const std::string& network_type,
                           const base::string16& title,
                           const base::string16& message,
                           const base::Closure& callback) {
  NET_LOG(ERROR) << "ShowErrorNotification: " << service_path << ": "
                 << base::UTF16ToUTF8(title);
  const gfx::Image& icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          GetErrorNotificationIconId(network_type));
  message_center::MessageCenter::Get()->AddNotification(
      message_center::Notification::CreateSystemNotification(
          notification_id, title, message, icon,
          ui::NetworkStateNotifier::kNotifierNetworkError, callback));
}

}  // namespace

namespace ui {

const char NetworkStateNotifier::kNotifierNetwork[] = "ui.chromeos.network";
const char NetworkStateNotifier::kNotifierNetworkError[] =
    "ui.chromeos.network.error";

const char NetworkStateNotifier::kNetworkConnectNotificationId[] =
    "chrome://settings/internet/connect";
const char NetworkStateNotifier::kNetworkActivateNotificationId[] =
    "chrome://settings/internet/activate";
const char NetworkStateNotifier::kNetworkOutOfCreditsNotificationId[] =
    "chrome://settings/internet/out-of-credits";

NetworkStateNotifier::NetworkStateNotifier(NetworkConnect* network_connect)
    : network_connect_(network_connect),
      did_show_out_of_credits_(false),
      weak_ptr_factory_(this) {
  if (!NetworkHandler::IsInitialized())
    return;
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  handler->AddObserver(this, FROM_HERE);
  UpdateDefaultNetwork(handler->DefaultNetwork());
  NetworkHandler::Get()->network_connection_handler()->AddObserver(this);
}

NetworkStateNotifier::~NetworkStateNotifier() {
  if (!NetworkHandler::IsInitialized())
    return;
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                 FROM_HERE);
  NetworkHandler::Get()->network_connection_handler()->RemoveObserver(this);
}

void NetworkStateNotifier::ConnectToNetworkRequested(
    const std::string& service_path) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (network && network->type() == shill::kTypeVPN)
    connected_vpn_.clear();

  RemoveConnectNotification();
}

void NetworkStateNotifier::ConnectSucceeded(const std::string& service_path) {
  RemoveConnectNotification();
}

void NetworkStateNotifier::ConnectFailed(const std::string& service_path,
                                         const std::string& error_name) {
  // Only show a notification for certain errors. Other failures are expected
  // to be handled by the UI that initiated the connect request.
  // Note: kErrorConnectFailed may also cause the configure dialog to be
  // displayed, but we rely on the notification system to show additional
  // details if available.
  if (error_name != NetworkConnectionHandler::kErrorConnectFailed &&
      error_name != NetworkConnectionHandler::kErrorNotFound &&
      error_name != NetworkConnectionHandler::kErrorConfigureFailed &&
      error_name != NetworkConnectionHandler::kErrorCertLoadTimeout) {
    return;
  }
  ShowNetworkConnectError(error_name, service_path);
}

void NetworkStateNotifier::DisconnectRequested(
    const std::string& service_path) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (network && network->type() == shill::kTypeVPN)
    connected_vpn_.clear();
}

void NetworkStateNotifier::DefaultNetworkChanged(const NetworkState* network) {
  if (!UpdateDefaultNetwork(network))
    return;
  // If the default network changes to another network, allow the out of
  // credits notification to be shown again. A delay prevents the notification
  // from being shown too frequently (see below).
  if (network)
    did_show_out_of_credits_ = false;
}

void NetworkStateNotifier::NetworkConnectionStateChanged(
    const NetworkState* network) {
  if (network->type() == shill::kTypeVPN)
    UpdateVpnConnectionState(network);
}

void NetworkStateNotifier::NetworkPropertiesUpdated(
    const NetworkState* network) {
  if (network->type() != shill::kTypeCellular)
    return;
  UpdateCellularOutOfCredits(network);
  UpdateCellularActivating(network);
}

bool NetworkStateNotifier::UpdateDefaultNetwork(const NetworkState* network) {
  std::string default_network_path;
  if (network)
    default_network_path = network->path();
  if (default_network_path != last_default_network_) {
    last_default_network_ = default_network_path;
    return true;
  }
  return false;
}

void NetworkStateNotifier::UpdateVpnConnectionState(const NetworkState* vpn) {
  if (vpn->path() == connected_vpn_) {
    if (!vpn->IsConnectedState()) {
      ShowVpnDisconnectedNotification(vpn);
      connected_vpn_.clear();
    }
  } else if (vpn->IsConnectedState()) {
    connected_vpn_ = vpn->path();
  }
}

void NetworkStateNotifier::UpdateCellularOutOfCredits(
    const NetworkState* cellular) {
  // Only display a notification if we are out of credits and have not already
  // shown a notification (or have since connected to another network type).
  if (!cellular->cellular_out_of_credits() || did_show_out_of_credits_)
    return;

  // Only display a notification if not connected, connecting, or waiting to
  // connect to another network.
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  const NetworkState* default_network = handler->DefaultNetwork();
  if (default_network && default_network != cellular)
    return;
  if (handler->ConnectingNetworkByType(NetworkTypePattern::NonVirtual()) ||
      NetworkHandler::Get()
          ->network_connection_handler()
          ->HasPendingConnectRequest())
    return;

  did_show_out_of_credits_ = true;
  base::TimeDelta dtime = base::Time::Now() - out_of_credits_notify_time_;
  if (dtime.InSeconds() > kMinTimeBetweenOutOfCreditsNotifySeconds) {
    out_of_credits_notify_time_ = base::Time::Now();
    base::string16 error_msg = l10n_util::GetStringFUTF16(
        IDS_NETWORK_OUT_OF_CREDITS_BODY, base::UTF8ToUTF16(cellular->name()));
    ShowErrorNotification(
        cellular->path(), kNetworkOutOfCreditsNotificationId, cellular->type(),
        l10n_util::GetStringUTF16(IDS_NETWORK_OUT_OF_CREDITS_TITLE), error_msg,
        base::Bind(&NetworkStateNotifier::ShowNetworkSettingsForPath,
                   weak_ptr_factory_.GetWeakPtr(), cellular->path()));
  }
}

void NetworkStateNotifier::UpdateCellularActivating(
    const NetworkState* cellular) {
  // Keep track of any activating cellular network.
  std::string activation_state = cellular->activation_state();
  if (activation_state == shill::kActivationStateActivating) {
    cellular_activating_.insert(cellular->path());
    return;
  }
  // Only display a notification if this network was activating and is now
  // activated.
  if (!cellular_activating_.count(cellular->path()) ||
      activation_state != shill::kActivationStateActivated)
    return;

  cellular_activating_.erase(cellular->path());
  int icon_id;
  if (cellular->network_technology() == shill::kNetworkTechnologyLte)
    icon_id = IDR_AURA_UBER_TRAY_NOTIFICATION_LTE;
  else
    icon_id = IDR_AURA_UBER_TRAY_NOTIFICATION_3G;
  const gfx::Image& icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(icon_id);
  message_center::MessageCenter::Get()->AddNotification(
      message_center::Notification::CreateSystemNotification(
          kNetworkActivateNotificationId,
          l10n_util::GetStringUTF16(IDS_NETWORK_CELLULAR_ACTIVATED_TITLE),
          l10n_util::GetStringFUTF16(IDS_NETWORK_CELLULAR_ACTIVATED,
                                     base::UTF8ToUTF16((cellular->name()))),
          icon, kNotifierNetwork,
          base::Bind(&NetworkStateNotifier::ShowNetworkSettingsForPath,
                     weak_ptr_factory_.GetWeakPtr(), cellular->path())));
}

void NetworkStateNotifier::ShowNetworkConnectError(
    const std::string& error_name,
    const std::string& service_path) {
  if (service_path.empty()) {
    base::DictionaryValue shill_properties;
    ShowConnectErrorNotification(error_name, service_path, shill_properties);
    return;
  }
  // Get the up-to-date properties for the network and display the error.
  NetworkHandler::Get()->network_configuration_handler()->GetShillProperties(
      service_path,
      base::Bind(&NetworkStateNotifier::ConnectErrorPropertiesSucceeded,
                 weak_ptr_factory_.GetWeakPtr(), error_name),
      base::Bind(&NetworkStateNotifier::ConnectErrorPropertiesFailed,
                 weak_ptr_factory_.GetWeakPtr(), error_name, service_path));
}

void NetworkStateNotifier::ShowMobileActivationError(
    const std::string& service_path) {
  const NetworkState* cellular =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (!cellular || cellular->type() != shill::kTypeCellular) {
    NET_LOG(ERROR) << "ShowMobileActivationError without Cellular network: "
                   << service_path;
    return;
  }
  message_center::MessageCenter::Get()->AddNotification(
      message_center::Notification::CreateSystemNotification(
          kNetworkActivateNotificationId,
          l10n_util::GetStringUTF16(IDS_NETWORK_ACTIVATION_ERROR_TITLE),
          l10n_util::GetStringFUTF16(IDS_NETWORK_ACTIVATION_NEEDS_CONNECTION,
                                     base::UTF8ToUTF16(cellular->name())),
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(
              IDR_AURA_UBER_TRAY_NETWORK_FAILED_CELLULAR),
          kNotifierNetworkError,
          base::Bind(&NetworkStateNotifier::ShowNetworkSettingsForPath,
                     weak_ptr_factory_.GetWeakPtr(), service_path)));
}

void NetworkStateNotifier::RemoveConnectNotification() {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  if (message_center) {
    message_center->RemoveNotification(kNetworkConnectNotificationId,
                                       false /* not by user */);
  }
}

void NetworkStateNotifier::ConnectErrorPropertiesSucceeded(
    const std::string& error_name,
    const std::string& service_path,
    const base::DictionaryValue& shill_properties) {
  std::string state;
  shill_properties.GetStringWithoutPathExpansion(shill::kStateProperty, &state);
  if (chromeos::NetworkState::StateIsConnected(state) ||
      chromeos::NetworkState::StateIsConnecting(state)) {
    // Network is no longer in an error state. This can happen if an
    // unexpected idle state transition occurs, see crbug.com/333955.
    return;
  }
  ShowConnectErrorNotification(error_name, service_path, shill_properties);
}

void NetworkStateNotifier::ConnectErrorPropertiesFailed(
    const std::string& error_name,
    const std::string& service_path,
    const std::string& shill_connect_error,
    scoped_ptr<base::DictionaryValue> shill_error_data) {
  base::DictionaryValue shill_properties;
  ShowConnectErrorNotification(error_name, service_path, shill_properties);
}

void NetworkStateNotifier::ShowConnectErrorNotification(
    const std::string& error_name,
    const std::string& service_path,
    const base::DictionaryValue& shill_properties) {
  base::string16 error = GetConnectErrorString(error_name);
  NET_LOG(DEBUG) << "Notify: " << service_path
                 << ": Connect error: " << error_name << ": "
                 << base::UTF16ToUTF8(error);
  if (error.empty()) {
    std::string shill_error;
    shill_properties.GetStringWithoutPathExpansion(shill::kErrorProperty,
                                                   &shill_error);
    if (!chromeos::NetworkState::ErrorIsValid(shill_error)) {
      shill_properties.GetStringWithoutPathExpansion(
          shill::kPreviousErrorProperty, &shill_error);
      NET_LOG(DEBUG) << "Notify: " << service_path
                     << ": Service.PreviousError: " << shill_error;
      if (!chromeos::NetworkState::ErrorIsValid(shill_error))
        shill_error.clear();
    } else {
      NET_LOG(DEBUG) << "Notify: " << service_path
                     << ": Service.Error: " << shill_error;
    }

    const NetworkState* network =
        NetworkHandler::Get()->network_state_handler()->GetNetworkState(
            service_path);
    if (network) {
      // Always log last_error, but only use it if shill_error is empty.
      // TODO(stevenjb): This shouldn't ever be necessary, but is kept here as
      // a failsafe since more information is better than less when debugging
      // and we have encountered some strange edge cases before.
      NET_LOG(DEBUG) << "Notify: " << service_path
                     << ": Network.last_error: " << network->last_error();
      if (shill_error.empty())
        shill_error = network->last_error();
    }

    if (ShillErrorIsIgnored(shill_error)) {
      NET_LOG(DEBUG) << "Notify: " << service_path
                     << ": Ignoring error: " << error_name;
      return;
    }

    error = network_connect_->GetShillErrorString(shill_error, service_path);
    if (error.empty()) {
      if (error_name == NetworkConnectionHandler::kErrorConnectFailed &&
          network && !network->connectable()) {
        // Connect failure on non connectable network with no additional
        // information. We expect the UI to show configuration UI so do not
        // show an additional (and unhelpful) notification.
        return;
      }
      error = l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_UNKNOWN);
    }
  }
  NET_LOG(ERROR) << "Notify: " << service_path
                 << ": Connect error: " + base::UTF16ToUTF8(error);

  std::string network_name =
      chromeos::shill_property_util::GetNameFromProperties(service_path,
                                                           shill_properties);
  std::string network_error_details;
  shill_properties.GetStringWithoutPathExpansion(shill::kErrorDetailsProperty,
                                                 &network_error_details);

  base::string16 error_msg;
  if (!network_error_details.empty()) {
    // network_name should't be empty if network_error_details is set.
    error_msg = l10n_util::GetStringFUTF16(
        IDS_NETWORK_CONNECTION_ERROR_MESSAGE_WITH_SERVER_MESSAGE,
        base::UTF8ToUTF16(network_name), error,
        base::UTF8ToUTF16(network_error_details));
  } else if (network_name.empty()) {
    error_msg = l10n_util::GetStringFUTF16(
        IDS_NETWORK_CONNECTION_ERROR_MESSAGE_NO_NAME, error);
  } else {
    error_msg =
        l10n_util::GetStringFUTF16(IDS_NETWORK_CONNECTION_ERROR_MESSAGE,
                                   base::UTF8ToUTF16(network_name), error);
  }

  std::string network_type;
  shill_properties.GetStringWithoutPathExpansion(shill::kTypeProperty,
                                                 &network_type);

  ShowErrorNotification(
      service_path, kNetworkConnectNotificationId, network_type,
      l10n_util::GetStringUTF16(IDS_NETWORK_CONNECTION_ERROR_TITLE), error_msg,
      base::Bind(&NetworkStateNotifier::ShowNetworkSettingsForPath,
                 weak_ptr_factory_.GetWeakPtr(), service_path));
}

void NetworkStateNotifier::ShowVpnDisconnectedNotification(
    const NetworkState* vpn) {
  base::string16 error_msg = l10n_util::GetStringFUTF16(
      IDS_NETWORK_VPN_CONNECTION_LOST_BODY, base::UTF8ToUTF16(vpn->name()));
  ShowErrorNotification(
      vpn->path(), kNetworkConnectNotificationId, shill::kTypeVPN,
      l10n_util::GetStringUTF16(IDS_NETWORK_VPN_CONNECTION_LOST_TITLE),
      error_msg, base::Bind(&NetworkStateNotifier::ShowNetworkSettingsForPath,
                            weak_ptr_factory_.GetWeakPtr(), vpn->path()));
}

void NetworkStateNotifier::ShowNetworkSettingsForPath(
    const std::string& service_path) {
  network_connect_->ShowNetworkSettingsForPath(service_path);
}

}  // namespace ui
