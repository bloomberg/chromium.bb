// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/shill_log_source.h"

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_event_log.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

constexpr char kNetworkDevices[] = "network_devices";
constexpr char kNetworkServices[] = "network_services";
constexpr char kServicePrefix[] = "/service/";
constexpr char kDevicePrefix[] = "/device/";

std::string GetString(const base::Value* value) {
  if (!value)
    return std::string();
  if (!value->is_string()) {
    LOG(ERROR) << "Bad string value: " << *value;
    return std::string();
  }
  return value->GetString();
}

// Masked properties match src/platform2/modem-utilities/connectivity.
// Note: We rely on intelligent anonymous replacements for IP and MAC addresses
// and BSSID in components/feedback/anonymizer_tool.cc.
constexpr std::array<const char*, 20> kMaskedList = {
    // Masked for devices only in ScrubAndExpandProperties:
    // shill::kAddress,
    shill::kCellularPPPUsernameProperty,
    shill::kEapAnonymousIdentityProperty,
    shill::kEapIdentityProperty,
    shill::kEapPinProperty,
    shill::kEapSubjectAlternativeNameMatchProperty,
    shill::kEapSubjectMatchProperty,
    shill::kEquipmentIdProperty,
    shill::kEsnProperty,
    shill::kIccidProperty,
    shill::kImeiProperty,
    shill::kImsiProperty,
    shill::kMdnProperty,
    shill::kMeidProperty,
    shill::kMinProperty,
    // Replaced with logging id for services only in ScrubAndExpandProperties:
    // shill::kName,
    shill::kPPPoEUsernameProperty,
    shill::kSSIDProperty,
    shill::kUsageURLProperty,
    shill::kWifiHexSsid,
    // UIData extracts properties into sub dictionaries, so look for the base
    // property names.
    "HexSSID",
    "Identity",
};

constexpr char kMaskedString[] = "*** MASKED ***";

// Recursively scrubs dictionaries, masking any values in kMaskedList.
void ScrubDictionary(base::Value* dict) {
  if (!dict->is_dict())
    return;
  for (auto entry : dict->DictItems()) {
    base::Value& value = entry.second;
    if (value.is_dict()) {
      ScrubDictionary(&entry.second);
    } else if (base::Contains(kMaskedList, entry.first) &&
               (!value.is_string() || !value.GetString().empty())) {
      entry.second = base::Value(kMaskedString);
    }
  }
}

}  // namespace

namespace system_logs {

ShillLogSource::ShillLogSource(bool scrub)
    : SystemLogsSource("Shill"), scrub_(scrub) {}

ShillLogSource::~ShillLogSource() = default;

void ShillLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(callback_.is_null());
  callback_ = std::move(callback);

  chromeos::ShillManagerClient::Get()->GetProperties(base::BindOnce(
      &ShillLogSource::OnGetManagerProperties, weak_ptr_factory_.GetWeakPtr()));
}

void ShillLogSource::OnGetManagerProperties(
    absl::optional<base::Value> result) {
  if (!result) {
    LOG(ERROR) << "ManagerPropertiesCallback Failed";
    std::move(callback_).Run(std::make_unique<SystemLogsResponse>());
    return;
  }

  const base::Value* devices = result->FindListKey(shill::kDevicesProperty);
  if (devices) {
    for (const base::Value& device : devices->GetList()) {
      std::string path = GetString(&device);
      if (path.empty())
        continue;
      device_paths_.insert(path);
      chromeos::ShillDeviceClient::Get()->GetProperties(
          dbus::ObjectPath(path),
          base::BindOnce(&ShillLogSource::OnGetDevice,
                         weak_ptr_factory_.GetWeakPtr(), path));
    }
  }

  const base::Value* services = result->FindListKey(shill::kServicesProperty);
  if (services) {
    for (const base::Value& service : services->GetList()) {
      std::string path = GetString(&service);
      if (path.empty())
        continue;
      service_paths_.insert(path);
      chromeos::ShillServiceClient::Get()->GetProperties(
          dbus::ObjectPath(path),
          base::BindOnce(&ShillLogSource::OnGetService,
                         weak_ptr_factory_.GetWeakPtr(), path));
    }
  }

  CheckIfDone();
}

void ShillLogSource::OnGetDevice(const std::string& device_path,
                                 absl::optional<base::Value> properties) {
  if (!properties) {
    LOG(ERROR) << "Get Device Properties Failed for : " << device_path;
  } else {
    AddDeviceAndRequestIPConfigs(device_path, *properties);
  }
  device_paths_.erase(device_path);
  CheckIfDone();
}

void ShillLogSource::AddDeviceAndRequestIPConfigs(
    const std::string& device_path,
    const base::Value& properties) {
  base::Value* device = devices_.SetKey(
      device_path, ScrubAndExpandProperties(device_path, properties));

  const base::Value* ip_configs =
      properties.FindListKey(shill::kIPConfigsProperty);
  if (!ip_configs)
    return;

  for (const base::Value& ip_config : ip_configs->GetList()) {
    std::string ip_config_path = GetString(&ip_config);
    if (ip_config_path.empty())
      continue;
    ip_config_paths_.insert(ip_config_path);
    chromeos::ShillIPConfigClient::Get()->GetProperties(
        dbus::ObjectPath(ip_config_path),
        base::BindOnce(&ShillLogSource::OnGetIPConfig,
                       weak_ptr_factory_.GetWeakPtr(), device_path,
                       ip_config_path));
  }
  if (!ip_config_paths_.empty()) {
    device->SetKey(shill::kIPConfigsProperty,
                   base::Value(base::Value::Type::DICTIONARY));
  }
}

void ShillLogSource::OnGetIPConfig(const std::string& device_path,
                                   const std::string& ip_config_path,
                                   absl::optional<base::Value> properties) {
  if (!properties) {
    LOG(ERROR) << "Get IPConfig Properties Failed for : " << device_path << ": "
               << ip_config_path;
  } else {
    AddIPConfig(device_path, ip_config_path, *properties);
  }
  // Erase a single matching entry.
  ip_config_paths_.erase(ip_config_paths_.find(ip_config_path));
  CheckIfDone();
}

void ShillLogSource::AddIPConfig(const std::string& device_path,
                                 const std::string& ip_config_path,
                                 const base::Value& properties) {
  base::Value* device = devices_.FindKey(device_path);
  DCHECK(device);
  base::Value* ip_configs = device->FindDictKey(shill::kIPConfigsProperty);
  DCHECK(ip_configs);
  ip_configs->SetKey(ip_config_path,
                     ScrubAndExpandProperties(ip_config_path, properties));
}

void ShillLogSource::OnGetService(const std::string& service_path,
                                  absl::optional<base::Value> properties) {
  if (!properties) {
    LOG(ERROR) << "Get Service Properties Failed for : " << service_path;
  } else {
    services_.SetKey(service_path,
                     ScrubAndExpandProperties(service_path, *properties));
  }
  service_paths_.erase(service_path);
  CheckIfDone();
}

base::Value ShillLogSource::ScrubAndExpandProperties(
    const std::string& object_path,
    const base::Value& properties) {
  DCHECK(properties.is_dict());
  base::Value dict = properties.Clone();

  // Convert UIData from a string to a dictionary.
  std::string* ui_data = dict.FindStringKey(shill::kUIDataProperty);
  if (ui_data) {
    base::Value ui_data_dict(chromeos::onc::ReadDictionaryFromJson(*ui_data));
    if (ui_data_dict.is_dict())
      dict.SetKey(shill::kUIDataProperty, std::move(ui_data_dict));
  }

  if (!scrub_)
    return dict;

  if (base::StartsWith(object_path, kServicePrefix,
                       base::CompareCase::SENSITIVE)) {
    std::string log_name = chromeos::NetworkPathId(object_path);  // Not PII
    dict.SetStringKey(shill::kNameProperty, log_name);
  } else if (base::StartsWith(object_path, kDevicePrefix,
                              base::CompareCase::SENSITIVE)) {
    // Only mask "Address" in the top level Device dictionary, not globally
    // (which would mask IPConfigs which get anonymized separately).
    if (dict.FindKey(shill::kAddressProperty))
      dict.SetStringKey(shill::kNameProperty, kMaskedString);
  }

  ScrubDictionary(&dict);
  return dict;
}

void ShillLogSource::CheckIfDone() {
  if (!device_paths_.empty() || !ip_config_paths_.empty() ||
      !service_paths_.empty()) {
    return;
  }

  std::map<std::string, std::string> response;
  std::string json;
  base::JSONWriter::WriteWithOptions(
      devices_, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  response[kNetworkDevices] = std::move(json);
  base::JSONWriter::WriteWithOptions(
      services_, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  response[kNetworkServices] = std::move(json);

  // Clear |devices_| and |services_|.
  devices_ = base::Value(base::Value::Type::DICTIONARY);
  services_ = base::Value(base::Value::Type::DICTIONARY);

  std::move(callback_).Run(
      std::make_unique<SystemLogsResponse>(std::move(response)));
}

}  // namespace system_logs
