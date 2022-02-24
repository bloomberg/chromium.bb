// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/network_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/esim_manager.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/webui/network_ui/network_diagnostics_resource_provider.h"
#include "ash/webui/network_ui/network_health_resource_provider.h"
#include "ash/webui/network_ui/traffic_counters_resource_provider.h"
#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/net/network_health/network_health_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/ash/system_tray_client_impl.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/chromeos/cellular_setup/cellular_setup_localized_strings_provider.h"
#include "chrome/browser/ui/webui/chromeos/internet_config_dialog.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/chromeos/network_logs_message_handler.h"
#include "chrome/browser/ui/webui/chromeos/onc_import_message_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/network_ui_resources.h"
#include "chrome/grit/network_ui_resources_map.h"
#include "chromeos/network/cellular_esim_profile_handler.h"
#include "chromeos/network/cellular_esim_profile_handler_impl.h"
#include "chromeos/network/cellular_esim_uninstall_handler.h"
#include "chromeos/network/cellular_utils.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/network_onc_utils.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/device_event_log/device_event_log.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/network_element_localized_strings_provider.h"

namespace chromeos {

namespace {

constexpr char kAddNetwork[] = "addNetwork";
constexpr char kDisableESimProfile[] = "disableActiveESimProfile";
constexpr char kGetNetworkProperties[] = "getShillNetworkProperties";
constexpr char kGetDeviceProperties[] = "getShillDeviceProperties";
constexpr char kGetEthernetEAP[] = "getShillEthernetEAP";
constexpr char kOpenCellularActivationUi[] = "openCellularActivationUi";
constexpr char kResetESimCache[] = "resetESimCache";
constexpr char kResetEuicc[] = "resetEuicc";
constexpr char kShowNetworkDetails[] = "showNetworkDetails";
constexpr char kShowNetworkConfig[] = "showNetworkConfig";
constexpr char kShowAddNewWifiNetworkDialog[] = "showAddNewWifi";
constexpr char kGetHostname[] = "getHostname";
constexpr char kSetHostname[] = "setHostname";

bool GetServicePathFromGuid(const std::string& guid,
                            std::string* service_path) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);
  if (!network)
    return false;
  *service_path = network->path();
  return true;
}

void SetDeviceProperties(base::Value* dictionary) {
  const std::string* device = dictionary->FindStringKey(shill::kDeviceProperty);
  if (!device)
    return;
  const DeviceState* device_state =
      NetworkHandler::Get()->network_state_handler()->GetDeviceState(*device);
  if (!device_state)
    return;

  base::Value device_dictionary(device_state->properties().Clone());
  if (!device_state->ip_configs().DictEmpty()) {
    // Convert IPConfig dictionary to a ListValue.
    base::Value ip_configs(base::Value::Type::LIST);
    for (auto iter : device_state->ip_configs().DictItems()) {
      ip_configs.Append(iter.second.Clone());
    }
    device_dictionary.SetKey(shill::kIPConfigsProperty, std::move(ip_configs));
  }
  if (!device_dictionary.DictEmpty())
    dictionary->SetKey(shill::kDeviceProperty, std::move(device_dictionary));
}

bool IsGuestModeActive() {
  return user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
         user_manager::UserManager::Get()->IsLoggedInAsPublicAccount();
}

// Get the euicc path for reset euicc operation. Return absl::nullopt if the
// reset euicc is not allowed, i.e: the user is in guest mode, admin enables
// restrict cellular network policy or a managed eSIM profile already installed.
absl::optional<dbus::ObjectPath> GetEuiccResetPath() {
  if (IsGuestModeActive()) {
    NET_LOG(ERROR) << "Couldn't reset EUICC in guest mode.";
    return absl::nullopt;
  }
  absl::optional<dbus::ObjectPath> euicc_path = chromeos::GetCurrentEuiccPath();
  if (!euicc_path) {
    NET_LOG(ERROR) << "No current EUICC. Unable to reset EUICC";
    return absl::nullopt;
  }
  const ManagedNetworkConfigurationHandler*
      managed_network_configuration_handler =
          NetworkHandler::Get()->managed_network_configuration_handler();
  if (!managed_network_configuration_handler)
    return absl::nullopt;
  if (managed_network_configuration_handler
          ->AllowOnlyPolicyCellularNetworks()) {
    NET_LOG(ERROR)
        << "Couldn't reset EUICC if admin restricts cellular networks.";
    return absl::nullopt;
  }
  NetworkStateHandler* network_state_handler =
      NetworkHandler::Get()->network_state_handler();
  if (!network_state_handler)
    return absl::nullopt;
  NetworkStateHandler::NetworkStateList state_list;
  network_state_handler->GetNetworkListByType(NetworkTypePattern::Cellular(),
                                              /*configured_only=*/false,
                                              /*visible_only=*/false,
                                              /*limit=*/0, &state_list);

  HermesEuiccClient::Properties* euicc_properties =
      HermesEuiccClient::Get()->GetProperties(*euicc_path);
  const std::string& eid = euicc_properties->eid().value();
  for (const NetworkState* network : state_list) {
    if (network->eid() == eid && network->IsManagedByPolicy()) {
      NET_LOG(ERROR)
          << "Couldn't reset EUICC if a managed eSIM profile is installed.";
      return absl::nullopt;
    }
  }

  return euicc_path;
}

class NetworkDiagnosticsMessageHandler : public content::WebUIMessageHandler {
 public:
  NetworkDiagnosticsMessageHandler() = default;
  ~NetworkDiagnosticsMessageHandler() override = default;

  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "OpenFeedbackDialog",
        base::BindRepeating(
            &NetworkDiagnosticsMessageHandler::OpenFeedbackDialog,
            base::Unretained(this)));
  }

 private:
  void OpenFeedbackDialog(base::Value::ConstListView value) {
    chrome::ShowFeedbackPage(nullptr, chrome::kFeedbackSourceNetworkHealthPage,
                             "" /*description_template*/,
                             "" /*description_template_placeholder*/,
                             "network-health", "" /*extra_diagnostics*/);
  }
};

}  // namespace

namespace network_ui {

class NetworkConfigMessageHandler : public content::WebUIMessageHandler {
 public:
  NetworkConfigMessageHandler() {}

  NetworkConfigMessageHandler(const NetworkConfigMessageHandler&) = delete;
  NetworkConfigMessageHandler& operator=(const NetworkConfigMessageHandler&) =
      delete;

  ~NetworkConfigMessageHandler() override {}

  // WebUIMessageHandler implementation.
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        kAddNetwork,
        base::BindRepeating(&NetworkConfigMessageHandler::AddNetwork,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kGetNetworkProperties,
        base::BindRepeating(
            &NetworkConfigMessageHandler::GetShillNetworkProperties,
            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kGetDeviceProperties,
        base::BindRepeating(
            &NetworkConfigMessageHandler::GetShillDeviceProperties,
            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kGetEthernetEAP,
        base::BindRepeating(&NetworkConfigMessageHandler::GetShillEthernetEAP,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kOpenCellularActivationUi,
        base::BindRepeating(
            &NetworkConfigMessageHandler::OpenCellularActivationUi,
            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kResetESimCache,
        base::BindRepeating(&NetworkConfigMessageHandler::ResetESimCache,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kDisableESimProfile,
        base::BindRepeating(
            &NetworkConfigMessageHandler::DisableActiveESimProfile,
            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kResetEuicc,
        base::BindRepeating(&NetworkConfigMessageHandler::ResetEuicc,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kShowNetworkDetails,
        base::BindRepeating(&NetworkConfigMessageHandler::ShowNetworkDetails,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kShowNetworkConfig,
        base::BindRepeating(&NetworkConfigMessageHandler::ShowNetworkConfig,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kShowAddNewWifiNetworkDialog,
        base::BindRepeating(&NetworkConfigMessageHandler::ShowAddNewWifi,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kGetHostname,
        base::BindRepeating(&NetworkConfigMessageHandler::GetHostname,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kSetHostname,
        base::BindRepeating(&NetworkConfigMessageHandler::SetHostname,
                            base::Unretained(this)));
  }

 private:
  void Respond(const std::string& callback_id, const base::Value& response) {
    AllowJavascript();
    ResolveJavascriptCallback(base::Value(callback_id), response);
  }

  void GetShillNetworkProperties(base::Value::ConstListView arg_list) {
    CHECK_EQ(2u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();
    std::string guid = arg_list[1].GetString();

    std::string service_path;
    if (!GetServicePathFromGuid(guid, &service_path)) {
      RunErrorCallback(callback_id, guid, kGetNetworkProperties,
                       "Error.InvalidNetworkGuid");
      return;
    }
    NetworkHandler::Get()->network_configuration_handler()->GetShillProperties(
        service_path,
        base::BindOnce(
            &NetworkConfigMessageHandler::OnGetShillNetworkProperties,
            weak_ptr_factory_.GetWeakPtr(), callback_id, guid));
  }

  void OnGetShillNetworkProperties(const std::string& callback_id,
                                   const std::string& guid,
                                   const std::string& service_path,
                                   absl::optional<base::Value> result) {
    if (!result) {
      RunErrorCallback(callback_id, guid, kGetNetworkProperties, "Error.DBus");
      return;
    }
    // Set the 'service_path' property for debugging.
    result->SetKey("service_path", base::Value(service_path));
    // Set the device properties for debugging.
    SetDeviceProperties(&result.value());
    base::ListValue return_arg_list;
    return_arg_list.Append(std::move(*result));
    Respond(callback_id, return_arg_list);
  }

  void GetShillDeviceProperties(base::Value::ConstListView arg_list) {
    CHECK_EQ(2u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();
    std::string type = arg_list[1].GetString();

    const DeviceState* device =
        NetworkHandler::Get()->network_state_handler()->GetDeviceStateByType(
            onc::NetworkTypePatternFromOncType(type));
    if (!device) {
      RunErrorCallback(callback_id, type, kGetDeviceProperties,
                       "Error.InvalidDeviceType");
      return;
    }
    NetworkHandler::Get()->network_device_handler()->GetDeviceProperties(
        device->path(),
        base::BindOnce(&NetworkConfigMessageHandler::OnGetShillDeviceProperties,
                       weak_ptr_factory_.GetWeakPtr(), callback_id, type));
  }

  void GetShillEthernetEAP(base::Value::ConstListView arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();

    NetworkStateHandler::NetworkStateList list;
    NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
        NetworkTypePattern::Primitive(shill::kTypeEthernetEap),
        true /* configured_only */, false /* visible_only */, 1 /* limit */,
        &list);

    if (list.empty()) {
      Respond(callback_id, base::Value(base::Value::Type::LIST));
      return;
    }
    const NetworkState* eap = list.front();
    base::Value properties(base::Value::Type::DICTIONARY);
    properties.SetStringKey("guid", eap->guid());
    properties.SetStringKey("name", eap->name());
    properties.SetStringKey("type", eap->type());
    base::Value response(base::Value::Type::LIST);
    response.Append(std::move(properties));
    Respond(callback_id, response);
  }

  void OpenCellularActivationUi(base::Value::ConstListView arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();

    const NetworkState* cellular_network =
        NetworkHandler::Get()->network_state_handler()->FirstNetworkByType(
            NetworkTypePattern::Cellular());
    if (cellular_network) {
      SystemTrayClientImpl::Get()->ShowSettingsCellularSetup(
          /*show_psim_flow=*/true);
    }
    base::Value response(base::Value::Type::LIST);
    response.Append(base::Value(cellular_network != nullptr));
    Respond(callback_id, response);
  }

  void ResetESimCache(base::Value::ConstListView arg_list) {
    CellularESimProfileHandler* handler =
        NetworkHandler::Get()->cellular_esim_profile_handler();
    if (!handler)
      return;

    CellularESimProfileHandlerImpl* handler_impl =
        static_cast<CellularESimProfileHandlerImpl*>(handler);
    handler_impl->ResetESimProfileCache();
  }

  void DisableActiveESimProfile(base::Value::ConstListView arg_list) {
    CellularESimProfileHandler* handler =
        NetworkHandler::Get()->cellular_esim_profile_handler();
    if (!handler)
      return;

    CellularESimProfileHandlerImpl* handler_impl =
        static_cast<CellularESimProfileHandlerImpl*>(handler);
    handler_impl->DisableActiveESimProfile();
  }

  void ResetEuicc(base::Value::ConstListView arg_list) {
    absl::optional<dbus::ObjectPath> euicc_path = GetEuiccResetPath();
    if (!euicc_path)
      return;

    CellularESimUninstallHandler* handler =
        NetworkHandler::Get()->cellular_esim_uninstall_handler();
    if (!handler)
      return;
    NET_LOG(EVENT) << "Executing reset EUICC on " << euicc_path->value();
    handler->ResetEuiccMemory(
        *euicc_path, base::BindOnce(&NetworkConfigMessageHandler::OnEuiccReset,
                                    base::Unretained(this)));
  }

  void OnEuiccReset(bool success) {
    if (!success) {
      NET_LOG(ERROR) << "Error occurred when resetting EUICC.";
    }
  }

  void ShowNetworkDetails(base::Value::ConstListView arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string guid = arg_list[0].GetString();

    InternetDetailDialog::ShowDialog(guid);
  }

  void ShowNetworkConfig(base::Value::ConstListView arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string guid = arg_list[0].GetString();

    InternetConfigDialog::ShowDialogForNetworkId(guid);
  }

  void ShowAddNewWifi(base::Value::ConstListView arg_list) {
    InternetConfigDialog::ShowDialogForNetworkType(::onc::network_type::kWiFi);
  }

  void OnGetShillDeviceProperties(const std::string& callback_id,
                                  const std::string& type,
                                  const std::string& device_path,
                                  absl::optional<base::Value> result) {
    if (!result) {
      RunErrorCallback(callback_id, type, kGetDeviceProperties,
                       "GetDeviceProperties failed");
      return;
    }

    // Set the 'device_path' property for debugging.
    result->SetKey("device_path", base::Value(device_path));

    base::ListValue return_arg_list;
    return_arg_list.Append(std::move(*result));
    Respond(callback_id, return_arg_list);
  }

  void GetHostname(base::Value::ConstListView arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string callback_id = arg_list[0].GetString();
    std::string hostname =
        NetworkHandler::Get()->network_state_handler()->hostname();
    Respond(callback_id, base::Value(hostname));
  }

  void SetHostname(base::Value::ConstListView arg_list) {
    CHECK_EQ(1u, arg_list.size());
    std::string hostname = arg_list[0].GetString();
    NET_LOG(USER) << "SET HOSTNAME: " << hostname;
    NetworkHandler::Get()->network_state_handler()->SetHostname(hostname);
  }

  void ErrorCallback(const std::string& callback_id,
                     const std::string& guid_or_type,
                     const std::string& function_name,
                     const std::string& error_name,
                     std::unique_ptr<base::DictionaryValue> /* error_data */) {
    RunErrorCallback(callback_id, guid_or_type, function_name, error_name);
  }

  void RunErrorCallback(const std::string& callback_id,
                        const std::string& guid_or_type,
                        const std::string& function_name,
                        const std::string& error_name) {
    NET_LOG(ERROR) << "Shill Error: " << error_name << " id=" << guid_or_type;
    base::ListValue return_arg_list;
    base::Value dictionary(base::Value::Type::DICTIONARY);
    std::string key = function_name == kGetDeviceProperties
                          ? shill::kTypeProperty
                          : shill::kGuidProperty;
    dictionary.SetKey(key, base::Value(guid_or_type));
    dictionary.SetKey("ShillError", base::Value(error_name));
    return_arg_list.Append(std::move(dictionary));
    Respond(callback_id, return_arg_list);
  }

  void AddNetwork(base::Value::ConstListView args) {
    DCHECK(!args.empty());
    std::string onc_type = args[0].GetString();
    InternetConfigDialog::ShowDialogForNetworkType(onc_type);
  }

  base::WeakPtrFactory<NetworkConfigMessageHandler> weak_ptr_factory_{this};
};

}  // namespace network_ui

// static
void NetworkUI::GetLocalizedStrings(base::DictionaryValue* localized_strings) {
  localized_strings->SetStringKey(
      "titleText", l10n_util::GetStringUTF16(IDS_NETWORK_UI_TITLE));

  localized_strings->SetStringKey(
      "generalTab", l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_GENERAL));
  localized_strings->SetStringKey(
      "networkHealthTab",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_NETWORK_HEALTH));
  localized_strings->SetStringKey(
      "networkLogsTab",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_NETWORK_LOGS));
  localized_strings->SetStringKey(
      "networkStateTab",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_NETWORK_STATE));
  localized_strings->SetStringKey(
      "networkSelectTab",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_TAB_NETWORK_SELECT));

  localized_strings->SetStringKey(
      "autoRefreshText",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_AUTO_REFRESH));
  localized_strings->SetStringKey(
      "deviceLogLinkText", l10n_util::GetStringUTF16(IDS_DEVICE_LOG_LINK_TEXT));
  localized_strings->SetStringKey(
      "networkRefreshText", l10n_util::GetStringUTF16(IDS_NETWORK_UI_REFRESH));
  localized_strings->SetStringKey(
      "clickToExpandText", l10n_util::GetStringUTF16(IDS_NETWORK_UI_EXPAND));
  localized_strings->SetStringKey(
      "propertyFormatText",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_PROPERTY_FORMAT));

  localized_strings->SetStringKey(
      "normalFormatOption",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_FORMAT_NORMAL));
  localized_strings->SetStringKey(
      "managedFormatOption",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_FORMAT_MANAGED));
  localized_strings->SetStringKey(
      "stateFormatOption",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_FORMAT_STATE));
  localized_strings->SetStringKey(
      "shillFormatOption",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_FORMAT_SHILL));

  localized_strings->SetStringKey(
      "dhcpHostnameLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_DHCP_HOSTNAME));
  localized_strings->SetStringKey(
      "globalPolicyLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_GLOBAL_POLICY));
  localized_strings->SetStringKey(
      "networkListsLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LISTS));
  localized_strings->SetStringKey(
      "networkHealthLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_HEALTH));
  localized_strings->SetStringKey(
      "networkDiagnosticsLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_DIAGNOSTICS));
  localized_strings->SetStringKey(
      "visibleNetworksLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_VISIBLE_NETWORKS));
  localized_strings->SetStringKey(
      "favoriteNetworksLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_FAVORITE_NETWORKS));
  localized_strings->SetStringKey(
      "ethernetEapNetworkLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_ETHERNET_EAP));
  localized_strings->SetStringKey(
      "devicesLabel", l10n_util::GetStringUTF16(IDS_NETWORK_UI_DEVICES));

  localized_strings->SetStringKey(
      "cellularActivationLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NO_CELLULAR_ACTIVATION_LABEL));
  localized_strings->SetStringKey(
      "cellularActivationButtonText",
      l10n_util::GetStringUTF16(
          IDS_NETWORK_UI_OPEN_CELLULAR_ACTIVATION_BUTTON_TEXT));
  localized_strings->SetStringKey(
      "noCellularErrorText",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NO_CELLULAR_ERROR_TEXT));

  localized_strings->SetStringKey(
      "resetESimCacheLabel",
      l10n_util::GetStringUTF16(
          IDS_NETWORK_UI_RESET_ESIM_PROFILES_BUTTON_TEXT));
  localized_strings->SetStringKey(
      "resetESimCacheButtonText",
      l10n_util::GetStringUTF16(
          IDS_NETWORK_UI_RESET_ESIM_PROFILES_BUTTON_TEXT));

  localized_strings->SetStringKey(
      "disableESimProfilesLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_DISABLE_ESIM_PROFILES_LABEL));
  localized_strings->SetStringKey(
      "disableActiveESimProfileButtonText",
      l10n_util::GetStringUTF16(
          IDS_NETWORK_UI_DISABLE_ACTIVE_ESIM_PROFILE_BUTTON_TEXT));

  localized_strings->SetStringKey(
      "resetEuiccLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_RESET_EUICC_LABEL));

  localized_strings->SetStringKey(
      "addNewWifiLabel",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_ADD_NEW_WIFI_LABEL));
  localized_strings->SetStringKey(
      "addNewWifiButtonText",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_ADD_NEW_WIFI_BUTTON_TEXT));

  localized_strings->SetStringKey(
      "importOncButtonText",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_IMPORT_ONC_BUTTON_TEXT));

  localized_strings->SetStringKey(
      "addWiFiListItemName",
      l10n_util::GetStringUTF16(IDS_NETWORK_ADD_WI_FI_LIST_ITEM_NAME));

  // Network logs
  localized_strings->SetStringKey(
      "networkLogsDescription",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_DESCRIPTION));
  localized_strings->SetStringKey(
      "networkLogsSystemLogs",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_SYSTEM_LOGS));
  localized_strings->SetStringKey(
      "networkLogsFilterPii",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_FILTER_PII));
  localized_strings->SetStringKey(
      "networkLogsPolicies",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_POLICIES));
  localized_strings->SetStringKey(
      "networkLogsDebugLogs",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_DEBUG_LOGS));
  localized_strings->SetStringKey(
      "networkLogsChromeLogs",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_CHROME_LOGS));
  localized_strings->SetStringKey(
      "networkLogsStoreButton",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_STORE_BUTTON));
  localized_strings->SetStringKey(
      "networkLogsStatus",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_STATUS));
  localized_strings->SetStringKey(
      "networkLogsDebuggingTitle",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_DEBUGGING_TITLE));
  localized_strings->SetStringKey(
      "networkLogsDebuggingDescription",
      l10n_util::GetStringUTF16(
          IDS_NETWORK_UI_NETWORK_LOGS_DEBUGGING_DESCRIPTION));
  localized_strings->SetStringKey(
      "networkLogsDebuggingNone",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_DEBUGGING_NONE));
  localized_strings->SetStringKey(
      "networkLogsDebuggingUnknown",
      l10n_util::GetStringUTF16(IDS_NETWORK_UI_NETWORK_LOGS_DEBUGGING_UNKNOWN));

  // Network Diagnostics
  localized_strings->SetStringKey(
      "NetworkDiagnosticsRunAll",
      l10n_util::GetStringUTF16(IDS_NETWORK_DIAGNOSTICS_RUN_ALL));
  localized_strings->SetStringKey(
      "NetworkDiagnosticsSendFeedback",
      l10n_util::GetStringUTF16(IDS_NETWORK_DIAGNOSTICS_SEND_FEEDBACK));
}

NetworkUI::NetworkUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  web_ui->AddMessageHandler(
      std::make_unique<network_ui::NetworkConfigMessageHandler>());
  web_ui->AddMessageHandler(std::make_unique<OncImportMessageHandler>());
  web_ui->AddMessageHandler(std::make_unique<NetworkLogsMessageHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<NetworkDiagnosticsMessageHandler>());

  // Enable extension API calls in the WebUI.
  extensions::TabHelper::CreateForWebContents(web_ui->GetWebContents());

  base::DictionaryValue localized_strings;
  GetLocalizedStrings(&localized_strings);

  content::WebUIDataSource* html =
      content::WebUIDataSource::Create(chrome::kChromeUINetworkHost);

  html->DisableTrustedTypesCSP();

  html->AddLocalizedStrings(localized_strings);
  network_health::AddResources(html);
  network_diagnostics::AddResources(html);
  cellular_setup::AddLocalizedStrings(html);
  cellular_setup::AddNonStringLoadTimeData(html);
  ui::network_element::AddLocalizedStrings(html);
  ui::network_element::AddOncLocalizedStrings(html);
  traffic_counters::AddResources(html);
  html->UseStringsJs();

  webui::SetupWebUIDataSource(
      html, base::make_span(kNetworkUiResources, kNetworkUiResourcesSize),
      IDR_NETWORK_UI_NETWORK_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html);
}

NetworkUI::~NetworkUI() {}

void NetworkUI::BindInterface(
    mojo::PendingReceiver<network_config::mojom::CrosNetworkConfig> receiver) {
  ash::GetNetworkConfigService(std::move(receiver));
}

void NetworkUI::BindInterface(
    mojo::PendingReceiver<network_health::mojom::NetworkHealthService>
        receiver) {
  network_health::NetworkHealthService::GetInstance()->BindHealthReceiver(
      std::move(receiver));
}

void NetworkUI::BindInterface(
    mojo::PendingReceiver<
        network_diagnostics::mojom::NetworkDiagnosticsRoutines> receiver) {
  network_health::NetworkHealthService::GetInstance()->BindDiagnosticsReceiver(
      std::move(receiver));
}

void NetworkUI::BindInterface(
    mojo::PendingReceiver<cellular_setup::mojom::ESimManager> receiver) {
  ash::GetESimManager(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(NetworkUI)

}  // namespace chromeos
