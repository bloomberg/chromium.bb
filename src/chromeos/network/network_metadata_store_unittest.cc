// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_connection_handler_impl.h"
#include "chromeos/network/network_metadata_observer.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

class TestNetworkMetadataObserver : public NetworkMetadataObserver {
 public:
  TestNetworkMetadataObserver() = default;
  ~TestNetworkMetadataObserver() override = default;

  // NetworkConnectionObserver
  void OnFirstConnectionToNetwork(const std::string& guid) override {
    connections_.insert(guid);
  }
  void OnNetworkUpdate(const std::string& guid,
                       const base::Value* set_properties) override {
    if (!updates_.contains(guid)) {
      updates_[guid] = 1;
    } else {
      updates_[guid]++;
    }
  }

  bool HasConnected(const std::string& guid) {
    return connections_.count(guid) != 0;
  }

  int GetNumberOfUpdates(const std::string& guid) {
    if (!updates_.contains(guid)) {
      return 0;
    }
    return updates_[guid];
  }

 private:
  std::set<std::string> connections_;
  base::flat_map<std::string, int> updates_;
};

class NetworkMetadataStoreTest : public ::testing::Test {
 public:
  NetworkMetadataStoreTest() {
    network_configuration_handler_ =
        NetworkConfigurationHandler::InitializeForTest(
            helper_.network_state_handler(),
            nullptr /* network_device_handler */);

    network_connection_handler_ =
        std::make_unique<NetworkConnectionHandlerImpl>();
    network_connection_handler_->Init(
        helper_.network_state_handler(), network_configuration_handler_.get(),
        /*managed_network_configuration_handler=*/nullptr,
        /*cellular_esim_connection_handler=*/nullptr);

    network_state_handler_ = helper_.network_state_handler();
    NetworkHandler::Initialize();
    user_prefs_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    device_prefs_ = std::make_unique<TestingPrefServiceSimple>();

    NetworkMetadataStore::RegisterPrefs(user_prefs_->registry());
    NetworkMetadataStore::RegisterPrefs(device_prefs_->registry());

    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    auto account_id = AccountId::FromUserEmail("account@test.com");
    auto second_account_id = AccountId::FromUserEmail("account2@test.com");
    primary_user_ = fake_user_manager->AddUser(account_id);
    secondary_user_ = fake_user_manager->AddUser(second_account_id);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    metadata_store_ = std::make_unique<NetworkMetadataStore>(
        network_configuration_handler_.get(), network_connection_handler_.get(),
        network_state_handler_, user_prefs_.get(), device_prefs_.get(),
        /*is_enterprise_enrolled=*/false);
    metadata_observer_ = std::make_unique<TestNetworkMetadataObserver>();
    metadata_store_->AddObserver(metadata_observer_.get());
  }

  NetworkMetadataStoreTest(const NetworkMetadataStoreTest&) = delete;
  NetworkMetadataStoreTest& operator=(const NetworkMetadataStoreTest&) = delete;

  ~NetworkMetadataStoreTest() override {
    network_state_handler_ = nullptr;
    metadata_store_.reset();
    metadata_observer_.reset();
    user_prefs_.reset();
    device_prefs_.reset();
    network_connection_handler_.reset();
    scoped_user_manager_.reset();
    network_configuration_handler_.reset();
    NetworkHandler::Shutdown();
  }

  void SetUp() override {
    SetIsEnterpriseEnrolled(false);
    LoginUser(primary_user_);
  }

  // This creates a new NetworkMetadataStore object.
  void SetIsEnterpriseEnrolled(bool is_enterprise_enrolled) {
    metadata_store_ = std::make_unique<NetworkMetadataStore>(
        network_configuration_handler_.get(), network_connection_handler_.get(),
        network_state_handler_, user_prefs_.get(), device_prefs_.get(),
        is_enterprise_enrolled);
    metadata_store_->AddObserver(metadata_observer_.get());
  }

  void LoginUser(const user_manager::User* user) {
    UserManager()->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                                true /* browser_restart */,
                                false /* is_child */);
    UserManager()->SwitchActiveUser(user->GetAccountId());
  }

  user_manager::FakeUserManager* UserManager() {
    return static_cast<user_manager::FakeUserManager*>(
        user_manager::UserManager::Get());
  }

  std::string ConfigureService(const std::string& shill_json_string) {
    return helper_.ConfigureService(shill_json_string);
  }

  sync_preferences::TestingPrefServiceSyncable* user_prefs() {
    return user_prefs_.get();
  }
  TestingPrefServiceSimple* device_prefs() { return device_prefs_.get(); }

  NetworkMetadataStore* metadata_store() { return metadata_store_.get(); }
  TestNetworkMetadataObserver* metadata_observer() {
    return metadata_observer_.get();
  }
  NetworkConnectionHandler* network_connection_handler() {
    return network_connection_handler_.get();
  }
  NetworkConfigurationHandler* network_configuration_handler() {
    return network_configuration_handler_.get();
  }
  NetworkStateHandler* network_state_handler() {
    return network_state_handler_;
  }
  void ResetStore() {
    metadata_store_ = std::make_unique<NetworkMetadataStore>(
        network_configuration_handler_.get(), network_connection_handler_.get(),
        network_state_handler_, user_prefs_.get(), device_prefs_.get(),
        /*is_enterprise_enrolled=*/false);
    metadata_observer_ = std::make_unique<TestNetworkMetadataObserver>();
    metadata_store_->AddObserver(metadata_observer_.get());
  }

 protected:
  const user_manager::User* primary_user_;
  const user_manager::User* secondary_user_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper helper_{false /* use_default_devices_and_services */};
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<NetworkConnectionHandler> network_connection_handler_;
  NetworkStateHandler* network_state_handler_;
  std::unique_ptr<TestingPrefServiceSimple> device_prefs_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> user_prefs_;
  std::unique_ptr<NetworkMetadataStore> metadata_store_;
  std::unique_ptr<TestNetworkMetadataObserver> metadata_observer_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

namespace {
const char* kGuid = "wifi0";
const char* kGuid1 = "wifi1";
const char* kConfigWifi0Connectable =
    "{ \"GUID\": \"wifi0\", \"Type\": \"wifi\", \"State\": \"idle\", "
    "  \"Connectable\": true }";
const char* kConfigWifi0HiddenUser =
    "{ \"GUID\": \"wifi0\", \"Type\": \"wifi\", \"State\": \"idle\", "
    "  \"Connectable\": true, \"Profile\": \"user_profile_path\", "
    "\"WiFi.HiddenSSID\": true }";
const char* kConfigWifi1HiddenUser =
    "{ \"GUID\": \"wifi1\", \"Type\": \"wifi\", \"State\": \"idle\", "
    "  \"Connectable\": true, \"Profile\": \"user_profile_path\", "
    "\"WiFi.HiddenSSID\": true }";
const char* kConfigWifi1Shared =
    "{ \"GUID\": \"wifi0\", \"Type\": \"wifi\", \"State\": \"idle\", "
    "  \"Connectable\": true, \"Profile\": \"/profile/default\" }";
const char kHasFixedHiddenNetworks[] =
    "metadata_store.has_fixed_hidden_networks";
}  // namespace

TEST_F(NetworkMetadataStoreTest, FirstConnect) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  ASSERT_TRUE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_FALSE(metadata_observer()->HasConnected(kGuid));
  network_connection_handler()->ConnectToNetwork(
      service_path, base::DoNothing(), base::DoNothing(),
      true /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_TRUE(metadata_observer()->HasConnected(kGuid));
}

TEST_F(NetworkMetadataStoreTest, FirstConnect_AfterBadPassword) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  network_state_handler()->SetErrorForTest(service_path,
                                           shill::kErrorBadPassphrase);
  metadata_store()->ConnectFailed(service_path, shill::kErrorConnectFailed);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_TRUE(metadata_store()->GetHasBadPassword(kGuid));
  ASSERT_FALSE(metadata_observer()->HasConnected(kGuid));
  base::RunLoop().RunUntilIdle();

  network_state_handler()->SetErrorForTest(service_path, "");
  network_connection_handler()->ConnectToNetwork(
      service_path, base::DoNothing(), base::DoNothing(),
      true /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_FALSE(metadata_store()->GetHasBadPassword(kGuid));
  ASSERT_TRUE(metadata_observer()->HasConnected(kGuid));
}

TEST_F(NetworkMetadataStoreTest, BadPassword_AfterSuccessfulConnection) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  network_connection_handler()->ConnectToNetwork(
      service_path, base::DoNothing(), base::DoNothing(),
      true /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_FALSE(metadata_store()->GetHasBadPassword(kGuid));
  ASSERT_TRUE(metadata_observer()->HasConnected(kGuid));

  network_state_handler()->SetErrorForTest(service_path,
                                           shill::kErrorBadPassphrase);
  metadata_store()->ConnectFailed(service_path, shill::kErrorConnectFailed);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(metadata_store()->GetHasBadPassword(kGuid));
}

TEST_F(NetworkMetadataStoreTest, ConfigurationCreated) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  metadata_store()->OnConfigurationCreated(service_path, kGuid);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_store()->GetIsCreatedByUser(kGuid));
}

TEST_F(NetworkMetadataStoreTest, ConfigurationCreated_HiddenNetwork) {
  metadata_store()->OnConfigurationCreated("service_path", kGuid);
  // Network only exists after the OnConfigurationCreated has been called.
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_store()->GetIsCreatedByUser(kGuid));
}

TEST_F(NetworkMetadataStoreTest, ConfigurationUpdated) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  network_connection_handler()->ConnectToNetwork(
      service_path, base::DoNothing(), base::DoNothing(),
      true /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);
  base::RunLoop().RunUntilIdle();
  metadata_store()->SetIsConfiguredBySync(kGuid);
  ASSERT_FALSE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_TRUE(metadata_store()->GetIsConfiguredBySync(kGuid));
  ASSERT_EQ(0, metadata_observer()->GetNumberOfUpdates(kGuid));

  base::DictionaryValue properties;
  properties.SetKey(shill::kSecurityProperty, base::Value(shill::kSecurityPsk));
  properties.SetKey(shill::kPassphraseProperty, base::Value("secret"));

  network_configuration_handler()->SetShillProperties(
      service_path, properties, base::DoNothing(), base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_FALSE(metadata_store()->GetIsConfiguredBySync(kGuid));
  ASSERT_EQ(1, metadata_observer()->GetNumberOfUpdates(kGuid));
}

TEST_F(NetworkMetadataStoreTest, SharedConfigurationUpdatedByOtherUser) {
  std::string service_path = ConfigureService(kConfigWifi1Shared);
  metadata_store()->OnConfigurationCreated(service_path, kGuid);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0, metadata_observer()->GetNumberOfUpdates(kGuid));
  ASSERT_FALSE(metadata_store()->GetIsFieldExternallyModified(
      kGuid, shill::kProxyConfigProperty));

  LoginUser(secondary_user_);

  base::DictionaryValue other_properties;
  other_properties.SetKey(shill::kAutoConnectProperty, base::Value(true));
  other_properties.SetKey(shill::kProxyConfigProperty,
                          base::Value("proxy_details"));

  network_configuration_handler()->SetShillProperties(
      service_path, other_properties, base::DoNothing(), base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_store()->GetIsFieldExternallyModified(
      kGuid, shill::kProxyConfigProperty));

  LoginUser(primary_user_);
  base::DictionaryValue owner_properties;
  owner_properties.SetKey(shill::kProxyConfigProperty,
                          base::Value("new_proxy_details"));

  network_configuration_handler()->SetShillProperties(
      service_path, owner_properties, base::DoNothing(), base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(metadata_store()->GetIsFieldExternallyModified(
      kGuid, shill::kProxyConfigProperty));
}

TEST_F(NetworkMetadataStoreTest, SharedConfigurationUpdated_NewPassword) {
  std::string service_path = ConfigureService(kConfigWifi1Shared);
  metadata_store()->OnConfigurationCreated(service_path, kGuid);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0, metadata_observer()->GetNumberOfUpdates(kGuid));
  ASSERT_TRUE(metadata_store()->GetIsCreatedByUser(kGuid));

  LoginUser(secondary_user_);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));

  base::DictionaryValue other_properties;
  other_properties.SetKey(shill::kPassphraseProperty, base::Value("pass2"));

  network_configuration_handler()->SetShillProperties(
      service_path, other_properties, base::DoNothing(), base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_store()->GetIsCreatedByUser(kGuid));

  LoginUser(primary_user_);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));
}

TEST_F(NetworkMetadataStoreTest, ConfigurationRemoved) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  network_connection_handler()->ConnectToNetwork(
      service_path, base::DoNothing(), base::DoNothing(),
      true /* check_error_state */, ConnectCallbackMode::ON_COMPLETED);
  base::RunLoop().RunUntilIdle();
  metadata_store()->SetIsConfiguredBySync(kGuid);
  ASSERT_FALSE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_TRUE(metadata_store()->GetIsConfiguredBySync(kGuid));

  network_configuration_handler()->RemoveConfiguration(
      service_path, /*remove_confirmer=*/absl::nullopt, base::DoNothing(),
      base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_FALSE(metadata_store()->GetIsConfiguredBySync(kGuid));
}

TEST_F(NetworkMetadataStoreTest, OwnOobeNetworks) {
  UserManager()->LogoutAllUsers();
  ConfigureService(kConfigWifi1Shared);
  base::RunLoop().RunUntilIdle();

  LoginUser(primary_user_);
  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));

  UserManager()->set_is_current_user_new(true);
  UserManager()->set_is_current_user_owner(true);
  metadata_store()->LoggedInStateChanged();
  ASSERT_TRUE(metadata_store()->GetIsCreatedByUser(kGuid));
}

TEST_F(NetworkMetadataStoreTest, OwnOobeNetworks_EnterpriseEnrolled) {
  SetIsEnterpriseEnrolled(true);
  UserManager()->LogoutAllUsers();
  ConfigureService(kConfigWifi1Shared);
  base::RunLoop().RunUntilIdle();

  LoginUser(primary_user_);
  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));

  UserManager()->set_is_current_user_new(true);
  UserManager()->set_is_current_user_owner(true);
  metadata_store()->LoggedInStateChanged();
  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));
}

TEST_F(NetworkMetadataStoreTest, OwnOobeNetworks_NotOwner) {
  UserManager()->LogoutAllUsers();
  ConfigureService(kConfigWifi1Shared);
  base::RunLoop().RunUntilIdle();

  LoginUser(primary_user_);
  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));

  UserManager()->set_is_current_user_new(true);
  UserManager()->set_is_current_user_owner(false);
  metadata_store()->LoggedInStateChanged();
  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));
}

TEST_F(NetworkMetadataStoreTest, OwnOobeNetworks_NotFirstLogin) {
  UserManager()->LogoutAllUsers();
  ConfigureService(kConfigWifi1Shared);
  base::RunLoop().RunUntilIdle();

  LoginUser(primary_user_);
  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));

  UserManager()->set_is_current_user_new(false);
  UserManager()->set_is_current_user_owner(true);
  metadata_store()->LoggedInStateChanged();
  ASSERT_FALSE(metadata_store()->GetIsCreatedByUser(kGuid));
}

TEST_F(NetworkMetadataStoreTest, FixSyncedHiddenNetworks) {
  std::string service_path = ConfigureService(kConfigWifi0HiddenUser);
  metadata_store()->OnConfigurationCreated(service_path, kGuid);
  base::RunLoop().RunUntilIdle();
  std::string service_path1 = ConfigureService(kConfigWifi1HiddenUser);
  metadata_store()->OnConfigurationCreated(service_path1, kGuid1);
  base::RunLoop().RunUntilIdle();

  metadata_store()->SetIsConfiguredBySync(kGuid);
  user_prefs()->SetBoolean(kHasFixedHiddenNetworks, false);

  ASSERT_TRUE(metadata_store()->GetIsCreatedByUser(kGuid));
  ASSERT_TRUE(metadata_store()->GetIsConfiguredBySync(kGuid));
  ASSERT_TRUE(
      network_state_handler()->GetNetworkStateFromGuid(kGuid)->hidden_ssid());
  ASSERT_TRUE(
      network_state_handler()->GetNetworkStateFromGuid(kGuid1)->hidden_ssid());

  base::HistogramTester tester;
  ResetStore();
  metadata_store()->NetworkListChanged();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(
      network_state_handler()->GetNetworkStateFromGuid(kGuid)->hidden_ssid());
  ASSERT_TRUE(
      network_state_handler()->GetNetworkStateFromGuid(kGuid1)->hidden_ssid());
  tester.ExpectBucketCount("Network.Wifi.Synced.Hidden.Fixed",
                           /*sample=*/1, /*expected_count=*/1);
}

TEST_F(NetworkMetadataStoreTest, LogHiddenNetworks) {
  std::string service_path = ConfigureService(kConfigWifi0HiddenUser);
  metadata_store()->OnConfigurationCreated(service_path, kGuid);
  base::RunLoop().RunUntilIdle();

  std::string service_path1 = ConfigureService(kConfigWifi1HiddenUser);
  metadata_store()->OnConfigurationCreated(service_path1, kGuid1);
  base::RunLoop().RunUntilIdle();

  metadata_store()->ConnectSucceeded(service_path);
  base::RunLoop().RunUntilIdle();

  base::HistogramTester tester;
  ResetStore();
  metadata_store()->NetworkListChanged();
  base::RunLoop().RunUntilIdle();

  // Wifi0 connected today (0 days ago)
  tester.ExpectBucketCount("Network.Shill.WiFi.Hidden.LastConnected",
                           /*sample=*/0, /*expected_count=*/1);
  tester.ExpectBucketCount("Network.Shill.WiFi.Hidden.EverConnected",
                           /*sample=*/true, /*expected_count=*/1);
  // Wifi1 never connected
  tester.ExpectBucketCount("Network.Shill.WiFi.Hidden.EverConnected",
                           /*sample=*/false, /*expected_count=*/1);
}

TEST_F(NetworkMetadataStoreTest, EnableAndDisableTrafficCountersAutoReset) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  const base::Value* value =
      metadata_store()->GetEnableTrafficCountersAutoReset(kGuid);
  EXPECT_EQ(nullptr, value);

  metadata_store()->SetEnableTrafficCountersAutoReset(kGuid, /*enable=*/true);
  base::RunLoop().RunUntilIdle();

  value = metadata_store()->GetEnableTrafficCountersAutoReset(kGuid);
  ASSERT_TRUE(value && value->is_bool());
  EXPECT_TRUE(value->GetBool());

  metadata_store()->SetEnableTrafficCountersAutoReset(kGuid, /*enable=*/false);
  base::RunLoop().RunUntilIdle();

  value = metadata_store()->GetEnableTrafficCountersAutoReset(kGuid);
  ASSERT_TRUE(value && value->is_bool());
  EXPECT_FALSE(value->GetBool());
}

TEST_F(NetworkMetadataStoreTest, SetTrafficCountersAutoResetDay) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  const base::Value* value =
      metadata_store()->GetDayOfTrafficCountersAutoReset(kGuid);
  EXPECT_EQ(nullptr, value);

  metadata_store()->SetDayOfTrafficCountersAutoReset(kGuid, /*day=*/5);
  base::RunLoop().RunUntilIdle();

  value = metadata_store()->GetDayOfTrafficCountersAutoReset(kGuid);
  ASSERT_TRUE(value && value->is_int());
  EXPECT_EQ(5, value->GetInt());

  metadata_store()->SetDayOfTrafficCountersAutoReset(kGuid, /*day=*/31);
  base::RunLoop().RunUntilIdle();

  value = metadata_store()->GetDayOfTrafficCountersAutoReset(kGuid);
  ASSERT_TRUE(value && value->is_int());
  EXPECT_EQ(31, value->GetInt());
}

}  // namespace chromeos
