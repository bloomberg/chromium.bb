// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind_helpers.h"
#include "base/test/task_environment.h"
#include "chromeos/constants/chromeos_pref_names.h"
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
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace test {

class TestNetworkMetadataObserver : public NetworkMetadataObserver {
 public:
  TestNetworkMetadataObserver() = default;
  ~TestNetworkMetadataObserver() override = default;

  // NetworkConnectionObserver
  void OnFirstConnectionToNetwork(const std::string& guid) override {
    connections_.insert(guid);
  }
  void OnNetworkUpdate(const std::string& guid,
                       base::DictionaryValue* set_properties) override {
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

    network_connection_handler_.reset(new NetworkConnectionHandlerImpl());
    network_connection_handler_->Init(helper_.network_state_handler(),
                                      network_configuration_handler_, nullptr);

    network_state_handler_ = helper_.network_state_handler();
    NetworkHandler::Initialize();
    user_prefs_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    device_prefs_ = std::make_unique<TestingPrefServiceSimple>();

    NetworkMetadataStore::RegisterPrefs(user_prefs_->registry());
    NetworkMetadataStore::RegisterPrefs(device_prefs_->registry());

    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    auto account_id = AccountId::FromUserEmail("account@test.com");
    const user_manager::User* user = fake_user_manager->AddUser(account_id);
    fake_user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                                    true /* browser_restart */,
                                    false /* is_child */);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    metadata_store_ = std::make_unique<NetworkMetadataStore>(
        network_configuration_handler_, network_connection_handler_.get(),
        network_state_handler_, user_prefs_.get(), device_prefs_.get());
    metadata_observer_ = std::make_unique<TestNetworkMetadataObserver>();
    metadata_store_->AddObserver(metadata_observer_.get());
  }

  ~NetworkMetadataStoreTest() override {
    network_state_handler_ = nullptr;
    metadata_store_.reset();
    metadata_observer_.reset();
    user_prefs_.reset();
    device_prefs_.reset();
    network_configuration_handler_ = nullptr;
    network_connection_handler_.reset();
    scoped_user_manager_.reset();
    NetworkHandler::Shutdown();
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
    return network_configuration_handler_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper helper_{false /* use_default_devices_and_services */};
  NetworkConfigurationHandler* network_configuration_handler_;
  std::unique_ptr<NetworkConnectionHandler> network_connection_handler_;
  NetworkStateHandler* network_state_handler_;
  std::unique_ptr<TestingPrefServiceSimple> device_prefs_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> user_prefs_;
  std::unique_ptr<NetworkMetadataStore> metadata_store_;
  std::unique_ptr<TestNetworkMetadataObserver> metadata_observer_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMetadataStoreTest);
};

namespace {
const char* kGuid = "wifi0";
const char* kConfigWifi0Connectable =
    "{ \"GUID\": \"wifi0\", \"Type\": \"wifi\", \"State\": \"idle\", "
    "  \"Connectable\": true }";
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

TEST_F(NetworkMetadataStoreTest, ConfigurationCreated) {
  std::string service_path = ConfigureService(kConfigWifi0Connectable);
  metadata_store()->OnConfigurationCreated(service_path, kGuid);
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
      service_path, base::DoNothing(), base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(metadata_store()->GetLastConnectedTimestamp(kGuid).is_zero());
  ASSERT_FALSE(metadata_store()->GetIsConfiguredBySync(kGuid));
}

}  // namespace test
}  // namespace chromeos
