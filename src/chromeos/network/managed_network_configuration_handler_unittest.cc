// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/components/onc/onc_validator.h"
#include "chromeos/dbus/hermes/hermes_clients.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/cellular_connection_handler.h"
#include "chromeos/network/cellular_esim_installer.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/cellular_policy_handler.h"
#include "chromeos/network/fake_network_connection_handler.h"
#include "chromeos/network/managed_network_configuration_handler_impl.h"
#include "chromeos/network/mock_network_state_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_policy_observer.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "chromeos/network/test_cellular_esim_profile_handler.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace test_utils = ::chromeos::onc::test_utils;
using base::test::DictionaryHasValues;

namespace chromeos {

namespace {

constexpr char kUser1[] = "user1";
constexpr char kUser1ProfilePath[] = "/profile/user1/shill";

// The GUID used by chromeos/components/test/data/onc/policy/*.{json,onc} files
// for a VPN.
constexpr char kTestGuidVpn[] = "{a3860e83-f03d-4cb1-bafa-b22c9e746950}";

// The GUID used by chromeos/components/test/data/onc/policy/*.{json,onc} files
// for a managed Wifi service.
constexpr char kTestGuidManagedWifi[] = "policy_wifi1";

// The GUID used by chromeos/components/test/data/onc/policy/policy_cellular.onc
// files for a managed Cellular service.
constexpr char kTestGuidManagedCellular[] = "policy_cellular";

// The GUID used by
// chromeos/components/test/data/onc/policy/policy_cellular_with_iccid.onc files
// for a managed Cellular service.
constexpr char kTestGuidManagedCellular2[] = "policy_cellular2";

// The GUID used by
// chromeos/components/test/data/onc/policy/policy_cellular_with_no_smdp.onc
// files for a managed Cellular service.
constexpr char kTestGuidManagedCellular3[] = "policy_cellular3";

// The GUID used by chromeos/components/test/data/onc/policy/*.{json,onc} files
// for an unmanaged Wifi service.
constexpr char kTestGuidUnmanagedWifi2[] = "wifi2";

// The GUID used by chromeos/components/test/data/onc/policy/*.{json,onc} files
// for a Wifi service.
constexpr char kTestGuidEthernetEap[] = "policy_ethernet_eap";

constexpr char kTestEuiccPath[] = "/org/chromium/Hermes/Euicc/0";
constexpr char kTestEid[] = "12345678901234567890123456789012";

// A valid but empty (no networks and no certificates) and unencrypted
// configuration.
constexpr char kEmptyUnencryptedConfiguration[] =
    "{\"Type\":\"UnencryptedConfiguration\",\"NetworkConfigurations\":[],"
    "\"Certificates\":[]}";

std::string PrettyJson(const base::DictionaryValue& value) {
  std::string pretty;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &pretty);
  return pretty;
}

void ErrorCallback(const std::string& error_name,
                   std::unique_ptr<base::DictionaryValue> error_data) {
  ADD_FAILURE() << "Unexpected error: " << error_name
                << " with associated data: \n"
                << PrettyJson(*error_data);
}

class TestNetworkProfileHandler : public NetworkProfileHandler {
 public:
  TestNetworkProfileHandler() { Init(); }

  TestNetworkProfileHandler(const TestNetworkProfileHandler&) = delete;
  TestNetworkProfileHandler& operator=(const TestNetworkProfileHandler&) =
      delete;
};

class TestNetworkPolicyObserver : public NetworkPolicyObserver {
 public:
  TestNetworkPolicyObserver() = default;

  TestNetworkPolicyObserver(const TestNetworkPolicyObserver&) = delete;
  TestNetworkPolicyObserver& operator=(const TestNetworkPolicyObserver&) =
      delete;

  void PoliciesApplied(const std::string& userhash) override {
    policies_applied_count_++;
  }

  int GetPoliciesAppliedCountAndReset() {
    int count = policies_applied_count_;
    policies_applied_count_ = 0;
    return count;
  }

 private:
  int policies_applied_count_ = 0;
};

}  // namespace

class ManagedNetworkConfigurationHandlerTest : public testing::Test {
 public:
  ManagedNetworkConfigurationHandlerTest() = default;
  ManagedNetworkConfigurationHandlerTest(
      const ManagedNetworkConfigurationHandlerTest&) = delete;
  ManagedNetworkConfigurationHandlerTest& operator=(
      const ManagedNetworkConfigurationHandlerTest&) = delete;

  ~ManagedNetworkConfigurationHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    shill_clients::InitializeFakes();
    hermes_clients::InitializeFakes();

    network_state_handler_ = MockNetworkStateHandler::InitializeForTest();
    network_device_handler_ = NetworkDeviceHandler::InitializeForTesting(
        network_state_handler_.get());
    network_profile_handler_ = NetworkProfileHandler::InitializeForTesting();
    network_configuration_handler_ =
        NetworkConfigurationHandler::InitializeForTest(
            network_state_handler_.get(), network_device_handler_.get());
    network_connection_handler_ =
        std::make_unique<FakeNetworkConnectionHandler>();
    cellular_inhibitor_ = std::make_unique<CellularInhibitor>();
    cellular_inhibitor_->Init(network_state_handler_.get(),
                              network_device_handler_.get());
    cellular_esim_profile_handler_ =
        std::make_unique<TestCellularESimProfileHandler>();
    cellular_esim_profile_handler_->Init(network_state_handler_.get(),
                                         cellular_inhibitor_.get());
    cellular_connection_handler_ =
        std::make_unique<CellularConnectionHandler>();
    cellular_connection_handler_->Init(network_state_handler_.get(),
                                       cellular_inhibitor_.get(),
                                       cellular_esim_profile_handler_.get());
    cellular_esim_installer_ = std::make_unique<CellularESimInstaller>();
    // TODO(crbug.com/1248229): Create fake cellular esim installer for test
    // setup.
    cellular_esim_installer_->Init(
        cellular_connection_handler_.get(), cellular_inhibitor_.get(),
        network_connection_handler_.get(), network_profile_handler_.get(),
        network_state_handler_.get());
    cellular_policy_handler_ = std::make_unique<CellularPolicyHandler>();

    // ManagedNetworkConfigurationHandlerImpl's ctor is private.
    managed_network_configuration_handler_.reset(
        new ManagedNetworkConfigurationHandlerImpl());

    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
    ::onc::RegisterProfilePrefs(user_prefs_.registry());
    ::onc::RegisterPrefs(local_state_.registry());

    ui_proxy_config_service_ = std::make_unique<UIProxyConfigService>(
        &user_prefs_, &local_state_, network_state_handler_.get(),
        network_profile_handler_.get());
    managed_network_configuration_handler_->Init(
        cellular_policy_handler_.get(), network_state_handler_.get(),
        network_profile_handler_.get(), network_configuration_handler_.get(),
        network_device_handler_.get(),
        nullptr /* no ProhibitedTechnologiesHandler */);
    managed_network_configuration_handler_->set_ui_proxy_config_service(
        ui_proxy_config_service_.get());
    managed_network_configuration_handler_->AddObserver(&policy_observer_);
    cellular_policy_handler_->Init(
        cellular_esim_installer_.get(), network_profile_handler_.get(),
        managed_network_configuration_handler_.get());

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // Run remaining tasks.
    base::RunLoop().RunUntilIdle();
    ResetManagedNetworkConfigurationHandler();
    cellular_policy_handler_.reset();
    cellular_esim_installer_.reset();
    cellular_esim_profile_handler_.reset();
    cellular_connection_handler_.reset();
    cellular_inhibitor_.reset();
    network_configuration_handler_.reset();
    ui_proxy_config_service_.reset();
    network_profile_handler_.reset();
    network_device_handler_.reset();
    network_state_handler_.reset();
    network_connection_handler_.reset();

    hermes_clients::Shutdown();
    shill_clients::Shutdown();
  }

  TestNetworkPolicyObserver* policy_observer() { return &policy_observer_; }

  ManagedNetworkConfigurationHandler* managed_handler() {
    return managed_network_configuration_handler_.get();
  }

  ShillServiceClient::TestInterface* GetShillServiceClient() {
    return ShillServiceClient::Get()->GetTestInterface();
  }

  ShillProfileClient::TestInterface* GetShillProfileClient() {
    return ShillProfileClient::Get()->GetTestInterface();
  }

  void InitializeStandardProfiles() {
    GetShillProfileClient()->AddProfile(kUser1ProfilePath, kUser1);
    GetShillProfileClient()->AddProfile(
        NetworkProfileHandler::GetSharedProfilePath(),
        std::string() /* no userhash */);
  }

  void InitializeEuicc() {
    HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
    HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath), kTestEid, /*is_active=*/true,
        /*physical_slot=*/0);
    base::RunLoop().RunUntilIdle();
  }

  void SetPolicy(::onc::ONCSource onc_source,
                 const std::string& userhash,
                 const std::string& path_to_onc) {
    base::Value policy =
        path_to_onc.empty()
            ? onc::ReadDictionaryFromJson(kEmptyUnencryptedConfiguration)
            : test_utils::ReadTestDictionaryValue(path_to_onc);

    onc::Validator validator(true,   // error_on_unknown_field
                             true,   // error_on_wrong_recommended
                             false,  // error_on_missing_field
                             true,   // managed_onc
                             true);  // log_warnings
    validator.SetOncSource(onc_source);
    onc::Validator::Result validation_result;
    std::unique_ptr<base::DictionaryValue> validated_policy =
        validator.ValidateAndRepairObject(&onc::kToplevelConfigurationSignature,
                                          policy, &validation_result);
    if (validation_result == onc::Validator::INVALID) {
      ADD_FAILURE() << "Network configuration invalid.";
      return;
    }

    base::ListValue network_configs;
    const base::Value* found_network_configs = validated_policy->FindKeyOfType(
        ::onc::toplevel_config::kNetworkConfigurations,
        base::Value::Type::LIST);
    if (found_network_configs) {
      for (const auto& network_config : found_network_configs->GetList()) {
        network_configs.Append(network_config.Clone());
      }
    }

    base::DictionaryValue global_config;
    const base::Value* found_global_config = validated_policy->FindKeyOfType(
        ::onc::toplevel_config::kGlobalNetworkConfiguration,
        base::Value::Type::DICTIONARY);
    if (found_global_config) {
      global_config = std::move(*base::DictionaryValue::From(
          base::Value::ToUniquePtrValue(found_global_config->Clone())));
    }

    managed_network_configuration_handler_->SetPolicy(
        onc_source, userhash, network_configs, global_config);
  }

  void SetUpEntry(const std::string& path_to_shill_json,
                  const std::string& profile_path,
                  const std::string& entry_path) {
    std::unique_ptr<base::DictionaryValue> entry =
        test_utils::ReadTestDictionary(path_to_shill_json);
    GetShillProfileClient()->AddEntry(profile_path, entry_path, *entry);
  }

  void ResetManagedNetworkConfigurationHandler() {
    if (!managed_network_configuration_handler_)
      return;
    managed_network_configuration_handler_->RemoveObserver(&policy_observer_);
    managed_network_configuration_handler_.reset();
  }

  bool PropertiesMatch(const base::Value& v1, const base::Value& v2) {
    if (v1 == v2)
      return true;
    // EXPECT_EQ does not recursively log dictionaries, so use LOG instead.
    LOG(ERROR) << "v1=" << v1;
    LOG(ERROR) << "v2=" << v2;
    return false;
  }

  void FastForwardProfileRefreshDelay() {
    const base::TimeDelta kProfileRefreshCallbackDelay =
        base::Milliseconds(150);

    // Connect can result in two profile refresh calls before and after
    // enabling profile. Fast forward by delay after refresh.
    task_environment_.FastForwardBy(2 * kProfileRefreshCallbackDelay);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestNetworkPolicyObserver policy_observer_;
  std::unique_ptr<MockNetworkStateHandler> network_state_handler_;
  std::unique_ptr<NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  std::unique_ptr<UIProxyConfigService> ui_proxy_config_service_;
  std::unique_ptr<ManagedNetworkConfigurationHandlerImpl>
      managed_network_configuration_handler_;
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<CellularConnectionHandler> cellular_connection_handler_;
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<TestCellularESimProfileHandler>
      cellular_esim_profile_handler_;
  std::unique_ptr<FakeNetworkConnectionHandler> network_connection_handler_;
  std::unique_ptr<CellularESimInstaller> cellular_esim_installer_;
  std::unique_ptr<CellularPolicyHandler> cellular_policy_handler_;

  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(ManagedNetworkConfigurationHandlerTest, RemoveIrrelevantFields) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_wifi1_with_redundant_fields.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(*expected_shill_properties));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManagedCellular) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kESimPolicy);
  InitializeStandardProfiles();
  InitializeEuicc();

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_cellular.json");

  SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
            "policy/policy_cellular.onc");
  FastForwardProfileRefreshDelay();
  base::RunLoop().RunUntilIdle();

  std::string service_path = GetShillServiceClient()->FindServiceMatchingGUID(
      kTestGuidManagedCellular);
  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(*expected_shill_properties));

  // Verify that applying a new cellular policy with same ICCID should update
  // the old shill configuration.
  SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
            "policy/policy_cellular_with_iccid.onc");
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(std::string(), GetShillServiceClient()->FindServiceMatchingGUID(
                               kTestGuidManagedCellular));
  service_path = GetShillServiceClient()->FindServiceMatchingGUID(
      kTestGuidManagedCellular2);
  const base::Value* properties2 =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties2);
  absl::optional<bool> auto_connect =
      properties2->FindBoolKey(shill::kAutoConnectProperty);
  ASSERT_TRUE(*auto_connect);
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyManagedCellularDisableFeatureFlag) {
  base::test::ScopedFeatureList feature_list;
  InitializeStandardProfiles();
  InitializeEuicc();
  // Verify that when eSIM policy feature flag is not set, applying managed
  // eSIM policy should not create a new shill configuration for it.
  feature_list.InitAndDisableFeature(ash::features::kESimPolicy);
  SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
            "policy/policy_cellular.onc");
  FastForwardProfileRefreshDelay();
  base::RunLoop().RunUntilIdle();
  std::string service_path = GetShillServiceClient()->FindServiceMatchingGUID(
      kTestGuidManagedCellular);
  ASSERT_EQ(service_path, std::string());
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyIgnoreNoSmdpManagedCellular) {
  base::test::ScopedFeatureList feature_list;
  InitializeStandardProfiles();
  InitializeEuicc();
  // Verify that applying managed eSIM policy with no SMDP address in the ONC
  // should not create a new shill configuration for it.
  feature_list.InitAndEnableFeature(ash::features::kESimPolicy);
  SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
            "policy/policy_cellular_with_no_smdp.onc");
  FastForwardProfileRefreshDelay();
  base::RunLoop().RunUntilIdle();
  std::string service_path = GetShillServiceClient()->FindServiceMatchingGUID(
      kTestGuidManagedCellular3);
  ASSERT_EQ(service_path, std::string());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManageUnconfigured) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(*expected_shill_properties));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, EnableManagedCredentialsWiFi) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_autoconnect_on_unconfigured_wifi1.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_wifi1_autoconnect.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(*expected_shill_properties));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, EnableManagedCredentialsVPN) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_autoconnect_on_unconfigured_vpn.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_vpn_autoconnect.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidVpn);
  ASSERT_FALSE(service_path.empty());
  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);
}

// Ensure that EAP settings for ethernet are matched with the right profile
// entry and written to the dedicated EthernetEAP service.
TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyManageUnmanagedEthernetEAP) {
  InitializeStandardProfiles();
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/"
          "shill_policy_on_unmanaged_ethernet_eap.json");

  GetShillServiceClient()->AddService(
      "eth_entry", std::string() /* guid */, std::string() /* name */,
      "etherneteap", std::string() /* state */, true /* visible */);
  GetShillProfileClient()->AddService(kUser1ProfilePath, "eth_entry");
  SetUpEntry("policy/shill_unmanaged_ethernet_eap.json", kUser1ProfilePath,
             "eth_entry");

  // Also setup an unrelated WiFi configuration to verify that the right entry
  // is matched.
  GetShillServiceClient()->AddService(
      "wifi_entry", std::string() /* guid */, "wifi1", shill::kTypeWifi,
      std::string() /* state */, true /* visible */);
  SetUpEntry("policy/shill_unmanaged_wifi1.json", kUser1ProfilePath,
             "wifi_entry");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_ethernet_eap.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidEthernetEap);
  ASSERT_FALSE(service_path.empty());
  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(*expected_shill_properties));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyIgnoreUnmodified) {
  InitializeStandardProfiles();

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());

  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json", kUser1ProfilePath,
             "some_entry_path");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, PolicyApplicationRunning) {
  InitializeStandardProfiles();

  EXPECT_FALSE(managed_handler()->IsAnyPolicyApplicationRunning());

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  managed_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY,
      std::string(),             // no userhash
      base::ListValue(),         // no device network policy
      base::DictionaryValue());  // no device global config

  EXPECT_TRUE(managed_handler()->IsAnyPolicyApplicationRunning());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(managed_handler()->IsAnyPolicyApplicationRunning());

  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json", kUser1ProfilePath,
             "some_entry_path");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_wifi1_update.onc");
  EXPECT_TRUE(managed_handler()->IsAnyPolicyApplicationRunning());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(managed_handler()->IsAnyPolicyApplicationRunning());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, UpdatePolicyAfterFinished) {
  InitializeStandardProfiles();

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());

  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json", kUser1ProfilePath,
             "some_entry_path");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_wifi1_update.onc");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, UpdatePolicyBeforeFinished) {
  InitializeStandardProfiles();

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  // Usually the first call will cause a profile entry to be created, which we
  // don't fake here.
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_wifi1_update.onc");

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyManageUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json", kUser1ProfilePath,
             "old_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_wifi1.json");

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is deleted.
  EXPECT_FALSE(GetShillProfileClient()->HasService("old_entry_path"));

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(*expected_shill_properties));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUpdateManagedNewGUID) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_wifi1.json", kUser1ProfilePath,
             "old_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_wifi1.json");

  // The passphrase isn't sent again, because it's configured by the user and
  // Shill doesn't send it on GetProperties calls.
  expected_shill_properties->RemoveKey(shill::kPassphraseProperty);
  expected_shill_properties->RemoveKey(shill::kPassphraseRequiredProperty);

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is deleted.
  EXPECT_FALSE(GetShillProfileClient()->HasService("old_entry_path"));

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(*expected_shill_properties));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUpdateManagedVPN) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_vpn.json", kUser1ProfilePath, "entry_path");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_vpn.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidVpn);
  ASSERT_FALSE(service_path.empty());
  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary("policy/shill_policy_on_managed_vpn.json");
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyUpdateManagedVPNOpenVPNPlusUi) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_vpn.json", kUser1ProfilePath, "entry_path");

  // Apply a policy that does not provide an authenticaiton type.
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_vpn_no_auth.onc");
  base::RunLoop().RunUntilIdle();

  // Apply additional configuration (e.g. from the UI). This includes password
  // and OTP which should be allowed when authentication type is not explicitly
  // set. See https://crbug.com/817617 for details.
  const NetworkState* network_state =
      network_state_handler_->GetNetworkStateFromGuid(kTestGuidVpn);
  ASSERT_TRUE(network_state);
  std::unique_ptr<base::DictionaryValue> ui_config =
      test_utils::ReadTestDictionary("policy/policy_vpn_ui.json");
  managed_network_configuration_handler_->SetProperties(
      network_state->path(), *ui_config, base::DoNothing(),
      base::BindOnce(&ErrorCallback));
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidVpn);
  ASSERT_FALSE(service_path.empty());
  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_managed_vpn_plus_ui.json");
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyUpdateManagedVPNL2TPIPsecPlusUi) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_vpn_ipsec.json", kUser1ProfilePath,
             "entry_path");

  // Apply the VPN L2TP-IPsec policy that will be updated.
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_vpn_ipsec.onc");
  base::RunLoop().RunUntilIdle();

  // Update the VPN L2TP-IPsec policy.
  const NetworkState* network_state =
      network_state_handler_->GetNetworkStateFromGuid(kTestGuidVpn);
  ASSERT_TRUE(network_state);
  std::unique_ptr<base::DictionaryValue> ui_config =
      test_utils::ReadTestDictionary("policy/policy_vpn_ipsec_ui.json");
  managed_network_configuration_handler_->SetProperties(
      network_state->path(), *ui_config, base::DoNothing(),
      base::BindOnce(&ErrorCallback));
  base::RunLoop().RunUntilIdle();

  // Get shill service properties after the update.
  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidVpn);
  ASSERT_FALSE(service_path.empty());
  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_managed_vpn_ipsec_plus_ui.json");
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       SetPolicyUpdateManagedVPNNoUserAuthType) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_managed_vpn.json", kUser1ProfilePath, "entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary("policy/shill_policy_on_managed_vpn.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_vpn_no_user_auth_type.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidVpn);
  ASSERT_FALSE(service_path.empty());
  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_EQ(*expected_shill_properties, *properties);
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyReapplyToManaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json", kUser1ProfilePath,
             "old_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unmanaged_wifi1.json");

  // The passphrase isn't sent again, because it's configured by the user and
  // Shill doesn't send it on GetProperties calls.
  expected_shill_properties->RemoveKey(shill::kPassphraseProperty);
  expected_shill_properties->RemoveKey(shill::kPassphraseRequiredProperty);

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  {
    std::string service_path =
        GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
    ASSERT_FALSE(service_path.empty());
    const base::Value* properties =
        GetShillServiceClient()->GetServiceProperties(service_path);
    ASSERT_TRUE(properties);
    EXPECT_THAT(*properties, DictionaryHasValues(*expected_shill_properties));
  }

  // If we apply the policy again, without change, then the Shill profile will
  // not be modified.
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  {
    std::string service_path =
        GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
    ASSERT_FALSE(service_path.empty());
    const base::Value* properties =
        GetShillServiceClient()->GetServiceProperties(service_path);
    ASSERT_TRUE(properties);
    EXPECT_THAT(*properties, DictionaryHasValues(*expected_shill_properties));
  }
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyUnmanageManaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_policy_on_unmanaged_wifi1.json", kUser1ProfilePath,
             "old_entry_path");

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            std::string() /* path_to_onc */);
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is deleted.
  EXPECT_FALSE(GetShillProfileClient()->HasService("old_entry_path"));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetEmptyPolicyIgnoreUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi1.json", kUser1ProfilePath,
             "old_entry_path");

  // Before setting policy, old_entry_path should exist.
  ASSERT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            std::string() /* path_to_onc */);
  base::RunLoop().RunUntilIdle();

  // Verify old_entry_path is kept.
  EXPECT_TRUE(GetShillProfileClient()->HasService("old_entry_path"));
  EXPECT_EQ(1, policy_observer()->GetPoliciesAppliedCountAndReset());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, SetPolicyIgnoreUnmanaged) {
  InitializeStandardProfiles();
  SetUpEntry("policy/shill_unmanaged_wifi2.json", kUser1ProfilePath,
             "wifi2_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(*expected_shill_properties));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, AutoConnectDisallowed) {
  InitializeStandardProfiles();

  // Setup an unmanaged network.
  SetUpEntry("policy/shill_unmanaged_wifi2.json", kUser1ProfilePath,
             "wifi2_entry_path");

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_disallow_autoconnect_on_unmanaged_wifi2.json");

  // Apply the user policy with global autoconnect config and expect that
  // autoconnect is disabled in the network's profile entry.
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_allow_only_policy_networks_to_autoconnect.onc");
  base::RunLoop().RunUntilIdle();

  std::string wifi2_service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidUnmanagedWifi2);
  ASSERT_FALSE(wifi2_service_path.empty());
  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(wifi2_service_path);
  ASSERT_TRUE(properties);
  EXPECT_TRUE(PropertiesMatch(*expected_shill_properties, *properties));

  // Verify that GetManagedProperties correctly augments the properties with the
  // global config from the user policy.
  // GetManagedProperties requires the device policy to be set or explicitly
  // unset.
  managed_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY,
      std::string(),             // no userhash
      base::ListValue(),         // no device network policy
      base::DictionaryValue());  // no device global config

  base::RunLoop get_properties_run_loop;
  std::unique_ptr<base::DictionaryValue> dictionary;
  managed_handler()->GetManagedProperties(
      kUser1, wifi2_service_path,
      base::BindOnce(
          [](std::unique_ptr<base::DictionaryValue>* dictionary_out,
             base::RepeatingClosure quit_closure,
             const std::string& service_path,
             absl::optional<base::Value> dictionary,
             absl::optional<std::string> error) {
            if (dictionary) {
              *dictionary_out = base::DictionaryValue::From(
                  base::Value::ToUniquePtrValue(std::move(*dictionary)));
            } else {
              FAIL();
            }
            quit_closure.Run();
          },
          &dictionary, get_properties_run_loop.QuitClosure()));

  get_properties_run_loop.Run();

  ASSERT_TRUE(dictionary.get());
  std::unique_ptr<base::DictionaryValue> expected_managed_onc =
      test_utils::ReadTestDictionary(
          "policy/"
          "managed_onc_disallow_autoconnect_on_unmanaged_wifi2.onc");
  EXPECT_TRUE(PropertiesMatch(*expected_managed_onc, *dictionary));
}

TEST_F(ManagedNetworkConfigurationHandlerTest, LateProfileLoading) {
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<base::DictionaryValue> expected_shill_properties =
      test_utils::ReadTestDictionary(
          "policy/shill_policy_on_unconfigured_wifi1.json");

  InitializeStandardProfiles();
  base::RunLoop().RunUntilIdle();

  std::string service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(service_path.empty());
  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(service_path);
  ASSERT_TRUE(properties);
  EXPECT_THAT(*properties, DictionaryHasValues(*expected_shill_properties));
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       ShutdownDuringPolicyApplication) {
  InitializeStandardProfiles();

  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");

  // Reset the network configuration manager after setting policy and before
  // calling RunUntilIdle to simulate shutdown during policy application.
  ResetManagedNetworkConfigurationHandler();
  base::RunLoop().RunUntilIdle();
}

TEST_F(ManagedNetworkConfigurationHandlerTest, AllowOnlyPolicyWiFiToConnect) {
  InitializeStandardProfiles();

  // Check transfer to NetworkStateHandler
  EXPECT_CALL(
      *network_state_handler_,
      UpdateBlockedWifiNetworks(true, false, std::vector<std::string>()))
      .Times(1);

  // Set 'AllowOnlyPolicyWiFiToConnect' policy and another arbitrary user
  // policy.
  SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
            "policy/policy_allow_only_policy_networks_to_connect.onc");
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_TRUE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->GetBlockedHexSSIDs().empty());
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       AllowOnlyPolicyWiFiToConnectIfAvailable) {
  InitializeStandardProfiles();

  // Check transfer to NetworkStateHandler
  EXPECT_CALL(
      *network_state_handler_,
      UpdateBlockedWifiNetworks(false, true, std::vector<std::string>()))
      .Times(1);

  // Set 'AllowOnlyPolicyWiFiToConnectIfAvailable' policy and another arbitrary
  // user policy.
  SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
      "policy/policy_allow_only_policy_networks_to_connect_if_available.onc");
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_TRUE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->GetBlockedHexSSIDs().empty());
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       AllowOnlyPolicyNetworksToAutoconnect) {
  InitializeStandardProfiles();

  // Check transfer to NetworkStateHandler
  EXPECT_CALL(
      *network_state_handler_,
      UpdateBlockedWifiNetworks(false, false, std::vector<std::string>()))
      .Times(1);

  // Set 'AllowOnlyPolicyNetworksToAutoconnect' policy and another arbitrary
  // user policy.
  SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
            "policy/policy_allow_only_policy_networks_to_autoconnect.onc");
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_TRUE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->GetBlockedHexSSIDs().empty());
}

TEST_F(ManagedNetworkConfigurationHandlerTest,
       AllowOnlyPolicyCellularNetworksToConnect) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kESimPolicy);
  InitializeStandardProfiles();
  InitializeEuicc();

  // Check transfer to NetworkStateHandler. Expect one call for each policy
  // application.
  EXPECT_CALL(*network_state_handler_, UpdateBlockedCellularNetworks(true))
      .Times(2);

  // Set 'AllowOnlyPolicyCellularNetworks' policy and another arbitrary cellular
  // policy.
  SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
            "policy/policy_allow_only_policy_cellular_networks.onc");
  SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
            "policy/policy_cellular.onc");
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_TRUE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_TRUE(managed_handler()->GetBlockedHexSSIDs().empty());
}

// Test deprecated BlacklistedHexSSIDs property.
TEST_F(ManagedNetworkConfigurationHandlerTest, GetBlacklistedHexSSIDs) {
  InitializeStandardProfiles();
  std::vector<std::string> blocked = {"476F6F676C65477565737450534B"};

  // Check transfer to NetworkStateHandler
  EXPECT_CALL(*network_state_handler_,
              UpdateBlockedWifiNetworks(false, false, blocked))
      .Times(1);

  // Set 'BlacklistedHexSSIDs' policy and another arbitrary user policy.
  SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
            "policy/policy_deprecated_blacklisted_hex_ssids.onc");
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_EQ(blocked, managed_handler()->GetBlockedHexSSIDs());
}

TEST_F(ManagedNetworkConfigurationHandlerTest, GetBlockedHexSSIDs) {
  InitializeStandardProfiles();
  std::vector<std::string> blocked = {"476F6F676C65477565737450534B"};

  // Check transfer to NetworkStateHandler
  EXPECT_CALL(*network_state_handler_,
              UpdateBlockedWifiNetworks(false, false, blocked))
      .Times(1);

  // Set 'BlockedHexSSIDs' policy and another arbitrary user policy.
  SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, std::string(),
            "policy/policy_blocked_hex_ssids.onc");
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1, "policy/policy_wifi1.onc");
  base::RunLoop().RunUntilIdle();

  // Check ManagedNetworkConfigurationHandler policy accessors.
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyCellularNetworks());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnect());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyWiFiToConnectIfAvailable());
  EXPECT_FALSE(managed_handler()->AllowOnlyPolicyNetworksToAutoconnect());
  EXPECT_EQ(blocked, managed_handler()->GetBlockedHexSSIDs());
}

// Proxy settings can come from different sources. Proxy enforced by user policy
// (provided by kProxy prefence) should have precedence over configurations set
// by ONC policy. This test verifies that the order of preference is respected.
TEST_F(ManagedNetworkConfigurationHandlerTest, ActiveProxySettingsPreference) {
  // Configure network.
  InitializeStandardProfiles();
  GetShillServiceClient()->AddService(
      "wifi_entry", std::string() /* guid */, "wifi1", shill::kTypeWifi,
      std::string() /* state */, true /* visible */);

  // Use proxy configured network.
  SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUser1,
            "policy/policy_wifi1_proxy.onc");
  base::RunLoop().RunUntilIdle();

  std::string wifi_service_path =
      GetShillServiceClient()->FindServiceMatchingGUID(kTestGuidManagedWifi);
  ASSERT_FALSE(wifi_service_path.empty());

  const base::Value* properties =
      GetShillServiceClient()->GetServiceProperties(wifi_service_path);
  ASSERT_TRUE(properties);

  managed_handler()->SetPolicy(
      ::onc::ONC_SOURCE_DEVICE_POLICY,
      std::string(),             // no userhash
      base::ListValue(),         // no device network policy
      base::DictionaryValue());  // no device global config

  std::unique_ptr<base::DictionaryValue> dictionary_before_pref,
      dictionary_after_pref;

  base::RunLoop get_initial_properties_run_loop;
  // Get properties and verify that proxy is used.
  managed_handler()->GetManagedProperties(
      kUser1, wifi_service_path,
      base::BindOnce(
          [](std::unique_ptr<base::DictionaryValue>* dictionary_out,
             base::RepeatingClosure quit_closure,
             const std::string& service_path,
             absl::optional<base::Value> dictionary,
             absl::optional<std::string> error) {
            if (dictionary) {
              *dictionary_out = base::DictionaryValue::From(
                  base::Value::ToUniquePtrValue(std::move(*dictionary)));
            } else {
              ADD_FAILURE() << error.value_or("Failed");
            }
            quit_closure.Run();
          },
          &dictionary_before_pref,
          get_initial_properties_run_loop.QuitClosure()));

  get_initial_properties_run_loop.Run();

  std::string policy_before_pref =
      dictionary_before_pref->FindPath("ProxySettings.Type.UserPolicy")
          ->GetString();
  ASSERT_TRUE(dictionary_before_pref.get());
  ASSERT_EQ(policy_before_pref, "PAC");

  // Set pref not to use proxy.
  base::Value policy_prefs_config = ProxyConfigDictionary::CreateDirect();
  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));

  base::RunLoop get_merged_properties_run_loop;
  // Fetch managed properties after preference is set.
  managed_handler()->GetManagedProperties(
      kUser1, wifi_service_path,
      base::BindOnce(
          [](std::unique_ptr<base::DictionaryValue>* dictionary_out,
             base::RepeatingClosure quit_closure,
             const std::string& service_path,
             absl::optional<base::Value> dictionary,
             absl::optional<std::string> error) {
            if (dictionary) {
              *dictionary_out = base::DictionaryValue::From(
                  base::Value::ToUniquePtrValue(std::move(*dictionary)));
            } else {
              ADD_FAILURE() << error.value_or("Failed");
            }
            quit_closure.Run();
          },
          &dictionary_after_pref,
          get_merged_properties_run_loop.QuitClosure()));

  get_merged_properties_run_loop.Run();

  std::string policy_after_pref =
      dictionary_after_pref->FindPath("ProxySettings.Type.UserPolicy")
          ->GetString();

  ASSERT_TRUE(dictionary_after_pref.get());
  ASSERT_NE(dictionary_before_pref, dictionary_after_pref);
  ASSERT_EQ(policy_after_pref, "Direct");
}

}  // namespace chromeos
