// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/network_test_helper.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chromeos/components/sync_wifi/network_type_conversions.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "components/account_id/account_id.h"
#include "components/onc/onc_pref_names.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"

namespace chromeos {

namespace sync_wifi {

NetworkTestHelper::NetworkTestHelper()
    : CrosNetworkConfigTestHelper(/*initialize= */ false) {
  LoginState::Initialize();
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
  PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
  ::onc::RegisterProfilePrefs(user_prefs_.registry());
  ::onc::RegisterPrefs(local_state_.registry());
  NetworkMetadataStore::RegisterPrefs(user_prefs_.registry());
  NetworkMetadataStore::RegisterPrefs(local_state_.registry());

  network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
  network_configuration_handler_ =
      base::WrapUnique<NetworkConfigurationHandler>(
          NetworkConfigurationHandler::InitializeForTest(
              network_state_helper_->network_state_handler(),
              network_device_handler_.get()));
  ui_proxy_config_service_ = std::make_unique<chromeos::UIProxyConfigService>(
      &user_prefs_, &local_state_,
      network_state_helper_->network_state_handler(),
      network_profile_handler_.get());
  managed_network_configuration_handler_ =
      ManagedNetworkConfigurationHandler::InitializeForTesting(
          network_state_helper_->network_state_handler(),
          network_profile_handler_.get(), network_device_handler_.get(),
          network_configuration_handler_.get(), ui_proxy_config_service_.get());
  managed_network_configuration_handler_->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY,
      /*userhash=*/std::string(),
      /*network_configs_onc=*/base::ListValue(),
      /*global_network_config=*/base::DictionaryValue());

  auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
  auto primary_account_id = AccountId::FromUserEmail("primary@test.com");
  auto secondary_account_id = AccountId::FromUserEmail("secondary@test.com");
  primary_user_ = fake_user_manager->AddUser(primary_account_id);
  secondary_user_ = fake_user_manager->AddUser(secondary_account_id);
  scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
      std::move(fake_user_manager));
  LoginUser(primary_user_);

  Initialize(managed_network_configuration_handler_.get());
}

NetworkTestHelper::~NetworkTestHelper() {
  LoginState::Shutdown();
  ui_proxy_config_service_.reset();
}

void NetworkTestHelper::SetUp() {
  NetworkHandler::Initialize();
  NetworkHandler::Get()->InitializePrefServices(&user_prefs_, &local_state_);
  network_state_helper_->ResetDevicesAndServices();
  network_state_helper_->profile_test()->AddProfile(
      network_state_helper_->UserHash(), std::string());

  base::RunLoop().RunUntilIdle();
}

void NetworkTestHelper::LoginUser(const user_manager::User* user) {
  auto* user_manager = static_cast<user_manager::FakeUserManager*>(
      user_manager::UserManager::Get());
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             true /* browser_restart */, false /* is_child */);
  user_manager->SwitchActiveUser(user->GetAccountId());
}

std::string NetworkTestHelper::ConfigureWiFiNetwork(const std::string& ssid,
                                                    bool is_secured,
                                                    bool in_profile,
                                                    bool has_connected,
                                                    bool owned_by_user,
                                                    bool configured_by_sync) {
  std::string security_entry =
      is_secured ? R"("SecurityClass": "psk", "Passphrase": "secretsauce", )"
                 : R"("SecurityClass": "none", )";
  std::string profile_entry = base::StringPrintf(
      R"("Profile": "%s", )",
      in_profile ? network_state_helper_->UserHash() : "/profile/default");
  std::string guid = base::StringPrintf("%s_guid", ssid.c_str());
  std::string service_path =
      network_state_helper_->ConfigureService(base::StringPrintf(
          R"({"GUID": "%s", "Type": "wifi", "SSID": "%s",
            %s "State": "ready", "Strength": 100,
            %s "AutoConnect": true, "Connectable": true})",
          guid.c_str(), ssid.c_str(), security_entry.c_str(),
          profile_entry.c_str()));

  base::RunLoop().RunUntilIdle();

  if (!in_profile) {
    if (owned_by_user) {
      NetworkHandler::Get()->network_metadata_store()->OnConfigurationCreated(
          service_path, guid);
    } else {
      LoginUser(secondary_user_);
      NetworkHandler::Get()->network_metadata_store()->OnConfigurationCreated(
          service_path, guid);
      LoginUser(primary_user_);
    }
  }

  if (has_connected) {
    NetworkHandler::Get()->network_metadata_store()->ConnectSucceeded(
        service_path);
  }

  if (configured_by_sync) {
    NetworkHandler::Get()->network_metadata_store()->SetIsConfiguredBySync(
        guid);
  }

  return guid;
}

NetworkStateTestHelper* NetworkTestHelper::network_state_test_helper() {
  return network_state_helper_.get();
}

}  // namespace sync_wifi

}  // namespace chromeos
