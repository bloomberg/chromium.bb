// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/proxy/ui_proxy_config_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kTestUserWifiGuid[] = "wifi1";
constexpr char kTestUserWifiConfig[] = R"({
    "GUID": "wifi1", "Type": "wifi", "Profile": "user_profile_path",
    "ProxySettings": {"Type": "WPAD"}})";

constexpr char kTestSharedWifiGuid[] = "wifi2";
constexpr char kTestSharedWifiConfig[] = R"({
    "GUID": "wifi2", "Type": "wifi", "Profile": "shared_profile_path"})";

constexpr char kTestUnconfiguredWifiGuid[] = "wifi2";
constexpr char kTestUnconfiguredWifiConfig[] = R"({
    "GUID": "wifi2", "Type": "wifi"})";

constexpr char kAugmentedOncValueTemplate[] =
    R"({"Active": $2, "Effective": "$1", "$1": $2, "UserEditable": $3})";

constexpr char kAugmentedOncValueWithUserSettingTemplate[] =
    R"({"Active": $2, "Effective": "$1", "$1": $2, "UserSetting": $3,
      "UserEditable": $4})";

std::string UserSettingOncValue(const std::string& value) {
  return base::ReplaceStringPlaceholders(
      kAugmentedOncValueTemplate, {"UserSetting", value, "true"}, nullptr);
}

std::string UserPolicyOncValue(const std::string& value) {
  return base::ReplaceStringPlaceholders(
      kAugmentedOncValueTemplate, {"UserPolicy", value, "false"}, nullptr);
}

std::string DevicePolicyOncValue(const std::string& value) {
  return base::ReplaceStringPlaceholders(
      kAugmentedOncValueTemplate, {"DevicePolicy", value, "false"}, nullptr);
}

std::string ExtensionControlledOncValue(const std::string& value) {
  return base::ReplaceStringPlaceholders(
      R"({"Active": $1,"Effective": "ActiveExtension",
          "UserEditable": false})",
      {value}, nullptr);
}

std::string UserPolicyAndUserSettingOncValue(const std::string& policy,
                                             const std::string& user_setting) {
  return base::ReplaceStringPlaceholders(
      kAugmentedOncValueWithUserSettingTemplate,
      {
          "UserPolicy",
          policy,
          user_setting,
          "false",
      },
      nullptr);
}

std::string ExtensionControlledAndUserSettingOncValue(
    const std::string& extension,
    const std::string& user_setting) {
  return base::ReplaceStringPlaceholders(
      R"({"Active": $1,"Effective": "ActiveExtension", "UserSetting": $2,
          "UserEditable": false})",
      {extension, user_setting}, nullptr);
}

}  // namespace

class UIProxyConfigServiceTest : public testing::Test {
 public:
  UIProxyConfigServiceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {
    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
    PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
    ::onc::RegisterProfilePrefs(user_prefs_.registry());
    ::onc::RegisterPrefs(local_state_.registry());
  }

  void SetUp() override {
    shill_clients::InitializeFakes();
    NetworkHandler::Initialize();
    ConfigureService(kTestUserWifiConfig);
    ConfigureService(kTestSharedWifiConfig);
    ConfigureService(kTestUnconfiguredWifiConfig);
  }

  void TearDown() override {
    NetworkHandler::Shutdown();
    shill_clients::Shutdown();
  }

  ~UIProxyConfigServiceTest() override = default;

  void ConfigureService(const std::string& shill_json_string) {
    std::unique_ptr<base::DictionaryValue> shill_json_dict =
        base::DictionaryValue::From(
            onc::ReadDictionaryFromJson(shill_json_string));
    ASSERT_TRUE(shill_json_dict);
    ShillManagerClient::Get()->ConfigureService(
        *shill_json_dict, base::DoNothing(),
        base::BindOnce([](const std::string& name, const std::string& msg) {}));
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<UIProxyConfigService> CreateServiceOffLocalState() {
    return std::make_unique<UIProxyConfigService>(
        nullptr, &local_state_, NetworkHandler::Get()->network_state_handler(),
        NetworkHandler::Get()->network_profile_handler());
  }

  std::unique_ptr<UIProxyConfigService> CreateServiceForUser() {
    return std::make_unique<UIProxyConfigService>(
        &user_prefs_, &local_state_,
        NetworkHandler::Get()->network_state_handler(),
        NetworkHandler::Get()->network_profile_handler());
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(UIProxyConfigServiceTest, UnknownNetwork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_FALSE(service->MergeEnforcedProxyConfig("unkown_network", &config));
  EXPECT_EQ(base::Value(base::Value::Type::DICTIONARY), config);
}

TEST_F(UIProxyConfigServiceTest, UserConfigOnly) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_FALSE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));
  EXPECT_EQ(base::Value(base::Value::Type::DICTIONARY), config);
}

TEST_F(UIProxyConfigServiceTest, LocalStatePrefIgnoredForUserService) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value policy_prefs_config =
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true);
  local_state_.SetManagedPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_FALSE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));
  EXPECT_EQ(base::Value(base::Value::Type::DICTIONARY), config);
}

TEST_F(UIProxyConfigServiceTest, LocalStatePolicyPrefForDeviceService) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceOffLocalState();

  base::Value policy_prefs_config =
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true);
  local_state_.SetManagedPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {DevicePolicyOncValue(R"("PAC")"),
       DevicePolicyOncValue(R"("http://pac/script.pac")")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest,
       UserPolicyProxyNotMergedForUnconfiguredNetork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceOffLocalState();

  base::Value policy_prefs_config =
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true);
  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_FALSE(
      service->MergeEnforcedProxyConfig(kTestUnconfiguredWifiGuid, &config));
  EXPECT_EQ(base::Value(base::Value::Type::DICTIONARY), config);
}

TEST_F(UIProxyConfigServiceTest, ExtensionProxyNotMergedForUnconfiguredNetork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceOffLocalState();

  base::Value extension_prefs_config =
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true);
  user_prefs_.SetExtensionPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(extension_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_FALSE(
      service->MergeEnforcedProxyConfig(kTestUnconfiguredWifiGuid, &config));
  EXPECT_EQ(base::Value(base::Value::Type::DICTIONARY), config);
}

TEST_F(UIProxyConfigServiceTest, OncPolicyNotMergedForUnfongiuredNetork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceOffLocalState();

  const std::string user_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestUnconfiguredWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::JSONReader::ReadDeprecated(user_onc_config));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_FALSE(
      service->MergeEnforcedProxyConfig(kTestUnconfiguredWifiGuid, &config));
  EXPECT_EQ(base::Value(base::Value::Type::DICTIONARY), config);
}

TEST_F(UIProxyConfigServiceTest, PacPolicyPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value policy_prefs_config =
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true);
  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserPolicyOncValue(R"("PAC")"),
       UserPolicyOncValue(R"("http://pac/script.pac")")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, AutoDetectPolicyPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value policy_prefs_config = ProxyConfigDictionary::CreateAutoDetect();
  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("WPAD")")}, nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, DirectPolicyPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value policy_prefs_config = ProxyConfigDictionary::CreateDirect();
  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("Direct")")}, nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, ManualPolicyPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value policy_prefs_config = ProxyConfigDictionary::CreateFixedServers(
      "http=proxy1:81;https=proxy2:81;socks=proxy3:81", "localhost");
  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1,
          "Manual": {
            "HTTPProxy": {"Host": $2, "Port": $5},
            "SecureHTTPProxy": {"Host": $3, "Port": $5},
            "SOCKS": {"Host": $4, "Port": $5}
          },
          "ExcludeDomains": $6
          })",
      {UserPolicyOncValue(R"("Manual")"), UserPolicyOncValue(R"("proxy1")"),
       UserPolicyOncValue(R"("proxy2")"), UserPolicyOncValue(R"("proxy3")"),
       UserPolicyOncValue("81"), UserPolicyOncValue(R"(["localhost"])")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, PartialManualPolicyPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value policy_prefs_config =
      ProxyConfigDictionary::CreateFixedServers("http=proxy1:81;", "");
  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1,
          "Manual": {
            "HTTPProxy": {"Host": $2, "Port": $3},
            "SecureHTTPProxy": {"Host": $4, "Port": $5},
            "SOCKS": {"Host": $4, "Port": $5}
          },
          "ExcludeDomains": $6
          })",
      {UserPolicyOncValue(R"("Manual")"), UserPolicyOncValue(R"("proxy1")"),
       UserPolicyOncValue("81"), UserPolicyOncValue(R"("")"),
       UserPolicyOncValue("0"), UserPolicyOncValue("[]")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, ManualPolicyPrefWithPacPreset) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value policy_prefs_config = ProxyConfigDictionary::CreateFixedServers(
      "http=proxy:80;https=proxy:80;socks=proxy:80", "localhost");
  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));

  std::string config_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserSettingOncValue(R"("PAC")"),
       UserSettingOncValue(R"("http://pac/test.script.pac")")},
      nullptr);
  std::unique_ptr<base::Value> config =
      base::JSONReader::ReadDeprecated(config_json);
  ASSERT_TRUE(config) << config_json;

  EXPECT_TRUE(
      service->MergeEnforcedProxyConfig(kTestUserWifiGuid, config.get()));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1,
          "Manual": {
            "HTTPProxy": {"Host": $2, "Port": $3},
            "SecureHTTPProxy": {"Host": $2, "Port": $3},
            "SOCKS": {"Host": $2, "Port": $3}
          },
          "PAC": $4,
          "ExcludeDomains": $5
         })",
      {UserPolicyAndUserSettingOncValue(R"("Manual")", R"("PAC")"),
       UserPolicyOncValue(R"("proxy")"), UserPolicyOncValue("80"),
       UserSettingOncValue(R"("http://pac/test.script.pac")"),
       UserPolicyOncValue(R"(["localhost"])")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, *config);
}

TEST_F(UIProxyConfigServiceTest, PacExtensionPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value extension_prefs_config =
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true);
  user_prefs_.SetExtensionPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(extension_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {ExtensionControlledOncValue(R"("PAC")"),
       ExtensionControlledOncValue(R"("http://pac/script.pac")")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, AutoDetectExtensionPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value extension_prefs_config =
      ProxyConfigDictionary::CreateAutoDetect();
  user_prefs_.SetExtensionPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(extension_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {ExtensionControlledOncValue(R"("WPAD")")}, nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, DirectExtensionPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value extension_prefs_config = ProxyConfigDictionary::CreateDirect();
  user_prefs_.SetExtensionPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(extension_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {ExtensionControlledOncValue(R"("Direct")")}, nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, ManualExtensionPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value extension_prefs_config =
      ProxyConfigDictionary::CreateFixedServers(
          "http=proxy1:81;https=proxy2:82;socks=proxy3:81", "localhost");
  user_prefs_.SetExtensionPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(extension_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1,
          "Manual": {
            "HTTPProxy": {"Host": $2, "Port": $3},
            "SecureHTTPProxy": {"Host": $4, "Port": $5},
            "SOCKS": {"Host": $6, "Port": $3}
          },
          "ExcludeDomains": $7
          })",
      {ExtensionControlledOncValue(R"("Manual")"),
       ExtensionControlledOncValue(R"("proxy1")"),
       ExtensionControlledOncValue("81"),
       ExtensionControlledOncValue(R"("proxy2")"),
       ExtensionControlledOncValue("82"),
       ExtensionControlledOncValue(R"("proxy3")"),
       ExtensionControlledOncValue(R"(["localhost"])")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, ExtensionProxyOverridesDefault) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value extension_prefs_config = ProxyConfigDictionary::CreatePacScript(
      "http://extension/script.pac", true);
  user_prefs_.SetExtensionPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(extension_prefs_config)));

  std::string config_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserSettingOncValue(R"("PAC")"),
       UserSettingOncValue(R"("http://default/script.pac")")},
      nullptr);
  std::unique_ptr<base::Value> config =
      base::JSONReader::ReadDeprecated(config_json);
  ASSERT_TRUE(config) << config_json;

  EXPECT_TRUE(
      service->MergeEnforcedProxyConfig(kTestUserWifiGuid, config.get()));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {ExtensionControlledAndUserSettingOncValue(R"("PAC")", R"("PAC")"),
       ExtensionControlledAndUserSettingOncValue(
           R"("http://extension/script.pac")",
           R"("http://default/script.pac")")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, *config);
}

TEST_F(UIProxyConfigServiceTest, PolicyPrefOverridesExtensionPref) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value policy_prefs_config =
      ProxyConfigDictionary::CreatePacScript("http://managed/script.pac", true);
  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));

  base::Value extension_prefs_config =
      ProxyConfigDictionary::CreateAutoDetect();
  user_prefs_.SetExtensionPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(extension_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserPolicyOncValue(R"("PAC")"),
       UserPolicyOncValue(R"("http://managed/script.pac")")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, PolicyPrefForSharedNetwork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value policy_prefs_config = ProxyConfigDictionary::CreateAutoDetect();
  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestSharedWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("WPAD")")}, nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, ExtensionPrefForSharedNetwork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value extension_prefs_config =
      ProxyConfigDictionary::CreateAutoDetect();
  user_prefs_.SetExtensionPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(extension_prefs_config)));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestSharedWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {ExtensionControlledOncValue(R"("WPAD")")}, nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, PacOncUserPolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "PAC",
           "PAC": "http://onc/script.pac"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::JSONReader::ReadDeprecated(onc_config));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserPolicyOncValue(R"("PAC")"),
       UserPolicyOncValue(R"("http://onc/script.pac")")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, AutoDetectOncUserPolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::JSONReader::ReadDeprecated(onc_config));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("WPAD")")}, nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

// Tests that ONC policy configured networks without proxy settings force Direct
// connection.
TEST_F(UIProxyConfigServiceTest, OncUserPolicyWithoutProxySettings) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "AutoConnect": false}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::JSONReader::ReadDeprecated(onc_config));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("Direct")")}, nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, DirectOncUserPolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi",
           "ProxySettings": {"Type": "Direct"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::JSONReader::ReadDeprecated(onc_config));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("Direct")")}, nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, ManualOncUserPolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {
           "Type": "Manual",
           "ExcludeDomains": ["foo.test", "localhost"],
           "Manual": {
             "HTTPProxy": {"Host": "proxy1", "Port": 81},
             "SecureHTTPProxy": {"Host": "proxy2", "Port": 82},
             "SOCKS": {"Host": "proxy3", "Port": 83}}}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::JSONReader::ReadDeprecated(onc_config));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1,
          "Manual": {
            "HTTPProxy": {"Host": $2, "Port": $3},
            "SecureHTTPProxy": {"Host": $4, "Port": $5},
            "SOCKS": {"Host": $7, "Port": $6}
          },
          "ExcludeDomains": $8
          })",
      {UserPolicyOncValue(R"("Manual")"), UserPolicyOncValue(R"("proxy1")"),
       UserPolicyOncValue("81"), UserPolicyOncValue(R"("proxy2")"),
       UserPolicyOncValue("82"), UserPolicyOncValue("83"),
       UserPolicyOncValue(R"("proxy3")"),
       UserPolicyOncValue(R"(["foo.test", "localhost"])")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, PartialManualOncUserPolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {
           "Type": "Manual",
           "Manual": {
             "HTTPProxy": {"Host": "proxy1", "Port": 81},
             "SOCKS": {"Host": "proxy4", "Port": 84}}}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::JSONReader::ReadDeprecated(onc_config));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1,
          "Manual": {
            "HTTPProxy": {"Host": $2, "Port": $3},
            "SOCKS": {"Host": $4, "Port": $5},
            "SecureHTTPProxy": {"Host": $6, "Port": $7}
          },
          "ExcludeDomains": $8
          })",
      {UserPolicyOncValue(R"("Manual")"), UserPolicyOncValue(R"("proxy1")"),
       UserPolicyOncValue("81"), UserPolicyOncValue(R"("proxy4")"),
       UserPolicyOncValue("84"), UserPolicyOncValue(R"("")"),
       UserPolicyOncValue("0"), UserPolicyOncValue(R"([])")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, OncDevicePolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "PAC",
           "PAC": "http://onc/script.pac"}}])",
      {kTestUserWifiGuid}, nullptr);
  local_state_.SetManagedPref(::onc::prefs::kDeviceOpenNetworkConfiguration,
                              base::JSONReader::ReadDeprecated(onc_config));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {DevicePolicyOncValue(R"("PAC")"),
       DevicePolicyOncValue(R"("http://onc/script.pac")")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, OncUserPolicyForSharedNetwork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestSharedWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::JSONReader::ReadDeprecated(onc_config));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestSharedWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("WPAD")")}, nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, OncDevicePolicyForSharedNetwork) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "PAC",
           "PAC": "http://onc/script.pac"}}])",
      {kTestSharedWifiGuid}, nullptr);
  local_state_.SetManagedPref(::onc::prefs::kDeviceOpenNetworkConfiguration,
                              base::JSONReader::ReadDeprecated(onc_config));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestSharedWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {DevicePolicyOncValue(R"("PAC")"),
       DevicePolicyOncValue(R"("http://onc/script.pac")")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, OncUserAndDevicePolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string user_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::JSONReader::ReadDeprecated(user_onc_config));

  const std::string device_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "PAC",
           "PAC": "http://onc/script.pac"}}])",
      {kTestUserWifiGuid}, nullptr);
  local_state_.SetManagedPref(
      ::onc::prefs::kDeviceOpenNetworkConfiguration,
      base::JSONReader::ReadDeprecated(device_onc_config));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1})", {UserPolicyOncValue(R"("WPAD")")}, nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, OncUserAndDevicePolicyBuiltOffLocalState) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceOffLocalState();

  const std::string user_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::JSONReader::ReadDeprecated(user_onc_config));

  const std::string device_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "PAC",
           "PAC": "http://onc/script.pac"}}])",
      {kTestUserWifiGuid}, nullptr);
  local_state_.SetManagedPref(
      ::onc::prefs::kDeviceOpenNetworkConfiguration,
      base::JSONReader::ReadDeprecated(device_onc_config));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {DevicePolicyOncValue(R"("PAC")"),
       DevicePolicyOncValue(R"("http://onc/script.pac")")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, OncUserPolicyOverridesUserSettings) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  const std::string user_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::JSONReader::ReadDeprecated(user_onc_config));

  std::string config_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserSettingOncValue(R"("PAC")"),
       UserSettingOncValue(R"("http://pac/test.script.pac")")},
      nullptr);
  std::unique_ptr<base::Value> config =
      base::JSONReader::ReadDeprecated(config_json);
  ASSERT_TRUE(config) << config_json;

  EXPECT_TRUE(
      service->MergeEnforcedProxyConfig(kTestUserWifiGuid, config.get()));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserPolicyAndUserSettingOncValue(R"("WPAD")", R"("PAC")"),
       UserSettingOncValue(R"("http://pac/test.script.pac")")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, *config);
}

TEST_F(UIProxyConfigServiceTest, PolicyPrefOverridesOncPolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value policy_prefs_config =
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true);
  user_prefs_.SetManagedPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));

  const std::string user_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::JSONReader::ReadDeprecated(user_onc_config));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {UserPolicyOncValue(R"("PAC")"),
       UserPolicyOncValue(R"("http://pac/script.pac")")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

TEST_F(UIProxyConfigServiceTest, ExtensionPrefOverridesOncPolicy) {
  std::unique_ptr<UIProxyConfigService> service = CreateServiceForUser();

  base::Value extension_prefs_config =
      ProxyConfigDictionary::CreatePacScript("http://pac/script.pac", true);
  user_prefs_.SetExtensionPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(extension_prefs_config)));

  const std::string user_onc_config = base::ReplaceStringPlaceholders(
      R"([{"GUID": "$1", "Type": "WiFi", "ProxySettings": {"Type": "WPAD"}}])",
      {kTestUserWifiGuid}, nullptr);
  user_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                             base::JSONReader::ReadDeprecated(user_onc_config));

  base::Value config(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(service->MergeEnforcedProxyConfig(kTestUserWifiGuid, &config));

  std::string expected_json = base::ReplaceStringPlaceholders(
      R"({"Type": $1, "PAC": $2})",
      {ExtensionControlledOncValue(R"("PAC")"),
       ExtensionControlledOncValue(R"("http://pac/script.pac")")},
      nullptr);
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(expected_json);
  ASSERT_TRUE(expected) << expected_json;
  EXPECT_EQ(*expected, config);
}

}  // namespace chromeos
