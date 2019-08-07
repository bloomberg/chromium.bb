// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state_test_helper.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_utils.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void FailErrorCallback(const std::string& error_name,
                       const std::string& error_message) {}

const char kUserHash[] = "user_hash";
const char kProfilePathShared[] = "shared_profile_path";
const char kProfilePathUser[] = "user_profile_path";

}  // namespace

NetworkStateTestHelper::NetworkStateTestHelper(
    bool use_default_devices_and_services)
    : weak_ptr_factory_(this) {
  if (!ShillManagerClient::Get()) {
    shill_clients::InitializeFakes();
    shill_clients_initialized_ = true;
  }
  manager_test_ = ShillManagerClient::Get()->GetTestInterface();
  profile_test_ = ShillProfileClient::Get()->GetTestInterface();
  device_test_ = ShillDeviceClient::Get()->GetTestInterface();
  service_test_ = ShillServiceClient::Get()->GetTestInterface();

  profile_test_->AddProfile(kProfilePathShared,
                            std::string() /* shared profile */);
  profile_test_->AddProfile(kProfilePathUser, kUserHash);
  base::RunLoop().RunUntilIdle();

  network_state_handler_ = NetworkStateHandler::InitializeForTest();

  if (!use_default_devices_and_services)
    ResetDevicesAndServices();
}

NetworkStateTestHelper::~NetworkStateTestHelper() {
  ShutdownNetworkState();
  if (shill_clients_initialized_)
    shill_clients::Shutdown();
}

void NetworkStateTestHelper::ShutdownNetworkState() {
  if (!network_state_handler_)
    return;
  network_state_handler_->Shutdown();
  base::RunLoop().RunUntilIdle();  // Process any pending updates
  network_state_handler_.reset();
}

void NetworkStateTestHelper::ResetDevicesAndServices() {
  base::RunLoop().RunUntilIdle();  // Process any pending updates
  ClearDevices();
  ClearServices();

  // A Wifi device should always exist and default to enabled.
  manager_test_->AddTechnology(shill::kTypeWifi, true /* enabled */);
  device_test_->AddDevice("/device/wifi1", shill::kTypeWifi, "wifi_device1");

  base::RunLoop().RunUntilIdle();
}

void NetworkStateTestHelper::ClearDevices() {
  device_test_->ClearDevices();
  base::RunLoop().RunUntilIdle();
}

void NetworkStateTestHelper::ClearServices() {
  service_test_->ClearServices();
  base::RunLoop().RunUntilIdle();
}

std::string NetworkStateTestHelper::ConfigureService(
    const std::string& shill_json_string) {
  last_created_service_path_.clear();

  std::unique_ptr<base::DictionaryValue> shill_json_dict =
      base::DictionaryValue::From(
          onc::ReadDictionaryFromJson(shill_json_string));
  if (!shill_json_dict) {
    LOG(ERROR) << "Error parsing json: " << shill_json_string;
    return last_created_service_path_;
  }

  // As a result of the ConfigureService() and RunUntilIdle() calls below,
  // ConfigureCallback() will be invoked before the end of this function, so
  // |last_created_service_path| will be set before it is returned. In
  // error cases, ConfigureCallback() will not run, resulting in "" being
  // returned from this function.
  ShillManagerClient::Get()->ConfigureService(
      *shill_json_dict,
      base::Bind(&NetworkStateTestHelper::ConfigureCallback,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&FailErrorCallback));
  base::RunLoop().RunUntilIdle();

  return last_created_service_path_;
}

void NetworkStateTestHelper::ConfigureCallback(const dbus::ObjectPath& result) {
  last_created_service_path_ = result.value();
}

std::string NetworkStateTestHelper::GetServiceStringProperty(
    const std::string& service_path,
    const std::string& key) {
  const base::DictionaryValue* properties =
      service_test_->GetServiceProperties(service_path);
  std::string result;
  if (properties)
    properties->GetStringWithoutPathExpansion(key, &result);
  return result;
}

void NetworkStateTestHelper::SetServiceProperty(const std::string& service_path,
                                                const std::string& key,
                                                const base::Value& value) {
  service_test_->SetServiceProperty(service_path, key, value);
  base::RunLoop().RunUntilIdle();
}

std::unique_ptr<NetworkState>
NetworkStateTestHelper::CreateStandaloneNetworkState(
    const std::string& id,
    const std::string& type,
    const std::string& connection_state,
    int signal_strength) {
  auto network = std::make_unique<NetworkState>(id);
  network->SetGuid(id);
  network->set_name(id);
  network->set_type(type);
  network->set_visible(true);
  network->SetConnectionState(connection_state);
  network->set_signal_strength(signal_strength);
  return network;
}

const char* NetworkStateTestHelper::ProfilePathShared() {
  return kProfilePathShared;
}

const char* NetworkStateTestHelper::ProfilePathUser() {
  return kProfilePathUser;
}

const char* NetworkStateTestHelper::UserHash() {
  return kUserHash;
}

}  // namespace chromeos
