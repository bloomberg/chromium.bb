// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_test_helper_base.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/dbus/hermes/hermes_clients.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_profile_handler.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void FailErrorCallback(const std::string& error_name,
                       const std::string& error_message) {}

const char kUserHash[] = "user_hash";
const char kProfilePathUser[] = "user_profile_path";

}  // namespace

NetworkTestHelperBase::NetworkTestHelperBase() {
  if (!ShillManagerClient::Get()) {
    shill_clients::InitializeFakes();
    shill_clients_initialized_ = true;
  }

  if (!HermesManagerClient::Get()) {
    hermes_clients::InitializeFakes();
    hermes_clients_initialized_ = true;
  }

  manager_test_ = ShillManagerClient::Get()->GetTestInterface();
  profile_test_ = ShillProfileClient::Get()->GetTestInterface();
  device_test_ = ShillDeviceClient::Get()->GetTestInterface();
  service_test_ = ShillServiceClient::Get()->GetTestInterface();
  ip_config_test_ = ShillIPConfigClient::Get()->GetTestInterface();

  hermes_euicc_test_ = HermesEuiccClient::Get()->GetTestInterface();
  hermes_manager_test_ = HermesManagerClient::Get()->GetTestInterface();
  hermes_profile_test_ = HermesProfileClient::Get()->GetTestInterface();
}

NetworkTestHelperBase::~NetworkTestHelperBase() {
  if (shill_clients_initialized_)
    shill_clients::Shutdown();
  if (hermes_clients_initialized_)
    hermes_clients::Shutdown();
}

void NetworkTestHelperBase::ResetDevicesAndServices() {
  base::RunLoop().RunUntilIdle();  // Process any pending updates
  ClearDevices();
  ClearServices();

  // A Wifi device should always exist and default to enabled.
  manager_test_->AddTechnology(shill::kTypeWifi, true /* enabled */);
  const char* kDevicePath = "/device/wifi1";
  device_test_->AddDevice(kDevicePath, shill::kTypeWifi, "wifi_device1");

  // Set initial IPConfigs for the wifi device. The IPConfigs are set up in
  // FakeShillManagerClient::SetupDefaultEnvironment() and do not get cleared.
  base::ListValue ip_configs;
  ip_configs.Append("ipconfig_v4_path");
  ip_configs.Append("ipconfig_v6_path");
  device_test_->SetDeviceProperty(kDevicePath, shill::kIPConfigsProperty,
                                  ip_configs,
                                  /*notify_changed=*/false);
  base::RunLoop().RunUntilIdle();
}

void NetworkTestHelperBase::ClearDevices() {
  device_test_->ClearDevices();
  base::RunLoop().RunUntilIdle();
}

void NetworkTestHelperBase::ClearServices() {
  service_test_->ClearServices();
  base::RunLoop().RunUntilIdle();
}

void NetworkTestHelperBase::ClearProfiles() {
  profile_test_->ClearProfiles();
  manager_test_->ClearProfiles();
}

std::string NetworkTestHelperBase::ConfigureService(
    const std::string& shill_json_string) {
  last_created_service_path_.clear();

  base::Value shill_json_dict = onc::ReadDictionaryFromJson(shill_json_string);
  if (!shill_json_dict.is_dict()) {
    LOG(ERROR) << "Error parsing json: " << shill_json_string;
    return last_created_service_path_;
  }

  // As a result of the ConfigureService() and RunUntilIdle() calls below,
  // ConfigureCallback() will be invoked before the end of this function, so
  // |last_created_service_path| will be set before it is returned. In
  // error cases, ConfigureCallback() will not run, resulting in "" being
  // returned from this function.
  ShillManagerClient::Get()->ConfigureService(
      shill_json_dict,
      base::BindOnce(&NetworkTestHelperBase::ConfigureCallback,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FailErrorCallback));
  base::RunLoop().RunUntilIdle();

  return last_created_service_path_;
}

void NetworkTestHelperBase::ConfigureCallback(const dbus::ObjectPath& result) {
  last_created_service_path_ = result.value();
}

std::string NetworkTestHelperBase::GetServiceStringProperty(
    const std::string& service_path,
    const std::string& key) {
  const base::Value* properties =
      service_test_->GetServiceProperties(service_path);
  if (properties) {
    const std::string* result = properties->FindStringKey(key);
    if (result)
      return *result;
  }
  return std::string();
}

void NetworkTestHelperBase::SetServiceProperty(const std::string& service_path,
                                               const std::string& key,
                                               const base::Value& value) {
  service_test_->SetServiceProperty(service_path, key, value);
  base::RunLoop().RunUntilIdle();
}

std::string NetworkTestHelperBase::GetProfileStringProperty(
    const std::string& profile_path,
    const std::string& key) {
  base::Value properties = profile_test_->GetProfileProperties(profile_path);
  std::string* result = properties.FindStringKey(key);
  if (result)
    return *result;
  return std::string();
}

void NetworkTestHelperBase::SetProfileProperty(const std::string& profile_path,
                                               const std::string& key,
                                               const base::Value& value) {
  ShillProfileClient::Get()->SetProperty(dbus::ObjectPath(profile_path), key,
                                         value, base::BindOnce([] {}),
                                         base::BindOnce(&FailErrorCallback));
  base::RunLoop().RunUntilIdle();
}

const char* NetworkTestHelperBase::ProfilePathUser() {
  return kProfilePathUser;
}

const char* NetworkTestHelperBase::UserHash() {
  return kUserHash;
}

void NetworkTestHelperBase::AddDefaultProfiles() {
  profile_test_->AddProfile(NetworkProfileHandler::GetSharedProfilePath(),
                            std::string() /* shared profile */);
  profile_test_->AddProfile(kProfilePathUser, kUserHash);
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
