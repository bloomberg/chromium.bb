// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_private_api.h"

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/chrome_desktop_report_request_helper.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/pref_names.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/test/test_reg_util_win.h"
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "base/nix/xdg_util.h"
#endif

namespace enterprise_reporting_private =
    ::extensions::api::enterprise_reporting_private;

namespace extensions {

#if !defined(OS_CHROMEOS)

namespace {

const char kFakeClientId[] = "fake-client-id";

}  // namespace

// Test for API enterprise.reportingPrivate.getDeviceId
class EnterpriseReportingPrivateGetDeviceIdTest : public ExtensionApiUnittest {
 public:
  EnterpriseReportingPrivateGetDeviceIdTest() = default;

  void SetClientId(const std::string& client_id) {
    storage_.SetClientId(client_id);
  }

 private:
  policy::FakeBrowserDMTokenStorage storage_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseReportingPrivateGetDeviceIdTest);
};

TEST_F(EnterpriseReportingPrivateGetDeviceIdTest, GetDeviceId) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceIdFunction>();
  SetClientId(kFakeClientId);
  std::unique_ptr<base::Value> id =
      RunFunctionAndReturnValue(function.get(), "[]");
  ASSERT_TRUE(id);
  ASSERT_TRUE(id->is_string());
  EXPECT_EQ(kFakeClientId, id->GetString());
}

TEST_F(EnterpriseReportingPrivateGetDeviceIdTest, DeviceIdNotExist) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceIdFunction>();
  SetClientId("");
  ASSERT_EQ(enterprise_reporting::kDeviceIdNotFound,
            RunFunctionAndReturnError(function.get(), "[]"));
}

// Test for API enterprise.reportingPrivate.getDeviceId
class EnterpriseReportingPrivateDeviceDataFunctionsTest
    : public ExtensionApiUnittest {
 public:
  EnterpriseReportingPrivateDeviceDataFunctionsTest() = default;

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    ASSERT_TRUE(fake_appdata_dir_.CreateUniqueTempDir());
    OverrideEndpointVerificationDirForTesting(fake_appdata_dir_.GetPath());
  }

 private:
  base::ScopedTempDir fake_appdata_dir_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseReportingPrivateDeviceDataFunctionsTest);
};

TEST_F(EnterpriseReportingPrivateDeviceDataFunctionsTest, StoreDeviceData) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateSetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> values = std::make_unique<base::ListValue>();
  values->AppendString("a");
  values->Append(
      std::make_unique<base::Value>(base::Value::BlobStorage({1, 2, 3})));
  extension_function_test_utils::RunFunction(function.get(), std::move(values),
                                             browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(function->GetResultList());
  EXPECT_EQ(0u, function->GetResultList()->GetSize());
  EXPECT_TRUE(function->GetError().empty());
}

TEST_F(EnterpriseReportingPrivateDeviceDataFunctionsTest, DeviceDataMissing) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> values = std::make_unique<base::ListValue>();
  values->AppendString("b");
  extension_function_test_utils::RunFunction(function.get(), std::move(values),
                                             browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(function->GetResultList());
  EXPECT_EQ(1u, function->GetResultList()->GetSize());
  EXPECT_TRUE(function->GetError().empty());

  const base::Value* single_result = nullptr;
  EXPECT_TRUE(function->GetResultList()->Get(0, &single_result));
  ASSERT_TRUE(single_result);
  ASSERT_TRUE(single_result->is_blob());
  EXPECT_EQ(base::Value::BlobStorage(), single_result->GetBlob());
}

TEST_F(EnterpriseReportingPrivateDeviceDataFunctionsTest, DeviceBadId) {
  auto set_function =
      base::MakeRefCounted<EnterpriseReportingPrivateSetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> set_values =
      std::make_unique<base::ListValue>();
  set_values->AppendString("a/b");
  set_values->Append(
      std::make_unique<base::Value>(base::Value::BlobStorage({1, 2, 3})));
  extension_function_test_utils::RunFunction(set_function.get(),
                                             std::move(set_values), browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(set_function->GetError().empty());

  // Try to read the directory as a file and should fail.
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> values = std::make_unique<base::ListValue>();
  values->AppendString("a");
  extension_function_test_utils::RunFunction(function.get(), std::move(values),
                                             browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(function->GetResultList());
  EXPECT_EQ(0u, function->GetResultList()->GetSize());
  EXPECT_FALSE(function->GetError().empty());
}

TEST_F(EnterpriseReportingPrivateDeviceDataFunctionsTest, RetrieveDeviceData) {
  auto set_function =
      base::MakeRefCounted<EnterpriseReportingPrivateSetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> set_values =
      std::make_unique<base::ListValue>();
  set_values->AppendString("c");
  set_values->Append(
      std::make_unique<base::Value>(base::Value::BlobStorage({1, 2, 3})));
  extension_function_test_utils::RunFunction(set_function.get(),
                                             std::move(set_values), browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(set_function->GetError().empty());

  auto get_function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> values = std::make_unique<base::ListValue>();
  values->AppendString("c");
  extension_function_test_utils::RunFunction(get_function.get(),
                                             std::move(values), browser(),
                                             extensions::api_test_utils::NONE);
  const base::Value* single_result = nullptr;
  ASSERT_TRUE(get_function->GetResultList());
  EXPECT_TRUE(get_function->GetResultList()->Get(0, &single_result));
  EXPECT_TRUE(get_function->GetError().empty());
  ASSERT_TRUE(single_result);
  ASSERT_TRUE(single_result->is_blob());
  EXPECT_EQ(base::Value::BlobStorage({1, 2, 3}), single_result->GetBlob());

  // Clear the data and check that it is gone.
  auto set_function2 =
      base::MakeRefCounted<EnterpriseReportingPrivateSetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> reset_values =
      std::make_unique<base::ListValue>();
  reset_values->AppendString("c");
  extension_function_test_utils::RunFunction(set_function2.get(),
                                             std::move(reset_values), browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(set_function2->GetError().empty());

  auto get_function2 =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceDataFunction>();
  std::unique_ptr<base::ListValue> values2 =
      std::make_unique<base::ListValue>();
  values2->AppendString("c");
  extension_function_test_utils::RunFunction(get_function2.get(),
                                             std::move(values2), browser(),
                                             extensions::api_test_utils::NONE);
  ASSERT_TRUE(get_function2->GetResultList());
  EXPECT_EQ(1u, get_function2->GetResultList()->GetSize());
  EXPECT_TRUE(get_function2->GetError().empty());

  EXPECT_TRUE(get_function2->GetResultList()->Get(0, &single_result));
  ASSERT_TRUE(single_result);
  ASSERT_TRUE(single_result->is_blob());
  EXPECT_EQ(base::Value::BlobStorage(), single_result->GetBlob());
}

// TODO(pastarmovj): Remove once implementation for the other platform exists.
#if defined(OS_WIN)

// Test for API enterprise.reportingPrivate.getDeviceId
class EnterpriseReportingPrivateGetPersistentSecretFunctionTest
    : public ExtensionApiUnittest {
 public:
  EnterpriseReportingPrivateGetPersistentSecretFunctionTest() = default;

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
#if defined(OS_WIN)
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
#endif
  }

 private:
#if defined(OS_WIN)
  registry_util::RegistryOverrideManager registry_override_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(
      EnterpriseReportingPrivateGetPersistentSecretFunctionTest);
};

TEST_F(EnterpriseReportingPrivateGetPersistentSecretFunctionTest, GetSecret) {
  auto function = base::MakeRefCounted<
      EnterpriseReportingPrivateGetPersistentSecretFunction>();
  std::unique_ptr<base::Value> result1 =
      RunFunctionAndReturnValue(function.get(), "[]");
  ASSERT_TRUE(result1);
  ASSERT_TRUE(result1->is_blob());
  auto generated_blob = result1->GetBlob();

  // Re-running should not change the secret.
  auto function2 = base::MakeRefCounted<
      EnterpriseReportingPrivateGetPersistentSecretFunction>();
  std::unique_ptr<base::Value> result2 =
      RunFunctionAndReturnValue(function2.get(), "[]");
  ASSERT_TRUE(result2);
  ASSERT_TRUE(result2->is_blob());
  ASSERT_EQ(generated_blob, result2->GetBlob());

  // Re-running should not change the secret even when force recreate is set.
  auto function3 = base::MakeRefCounted<
      EnterpriseReportingPrivateGetPersistentSecretFunction>();
  std::unique_ptr<base::Value> result3 =
      RunFunctionAndReturnValue(function3.get(), "[true]");
  ASSERT_TRUE(result3);
  ASSERT_TRUE(result3->is_blob());
  ASSERT_EQ(generated_blob, result3->GetBlob());

  const wchar_t kDefaultRegistryPath[] =
      L"SOFTWARE\\Google\\Endpoint Verification";
  const wchar_t kValueName[] = L"Safe Storage";

  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_CURRENT_USER, kDefaultRegistryPath, KEY_WRITE));
  // Mess up with the value.
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kValueName, 1337));

  // Re-running with no recreate enforcement should return an error.
  auto function4 = base::MakeRefCounted<
      EnterpriseReportingPrivateGetPersistentSecretFunction>();
  std::string error = RunFunctionAndReturnError(function4.get(), "[]");
  ASSERT_FALSE(error.empty());

  // Re=running should not change the secret even when force recreate is set.
  auto function5 = base::MakeRefCounted<
      EnterpriseReportingPrivateGetPersistentSecretFunction>();
  std::unique_ptr<base::Value> result5 =
      RunFunctionAndReturnValue(function5.get(), "[true]");
  ASSERT_TRUE(result5);
  ASSERT_TRUE(result5->is_blob());
  ASSERT_NE(generated_blob, result5->GetBlob());
}

#endif  // defined(OS_WIN)

using EnterpriseReportingPrivateGetDeviceInfoTest = ExtensionApiUnittest;

TEST_F(EnterpriseReportingPrivateGetDeviceInfoTest, GetDeviceInfo) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceInfoFunction>();
  std::unique_ptr<base::Value> device_info_value =
      RunFunctionAndReturnValue(function.get(), "[]");
  ASSERT_TRUE(device_info_value.get());

  enterprise_reporting_private::DeviceInfo info;
  ASSERT_TRUE(enterprise_reporting_private::DeviceInfo::Populate(
      *device_info_value, &info));

#if defined(OS_MAC)
  EXPECT_EQ("macOS", info.os_name);
#elif defined(OS_WIN)
  EXPECT_EQ("windows", info.os_name);
  EXPECT_FALSE(info.device_model.empty());
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar(base::nix::kXdgCurrentDesktopEnvVar, "XFCE");
  EXPECT_EQ("linux", info.os_name);
#else
  // Verify a stub implementation.
  EXPECT_EQ("stubOS", info.os_name);
  EXPECT_EQ("0.0.0.0", info.os_version);
  EXPECT_EQ("midnightshift", info.device_host_name);
  EXPECT_EQ("topshot", info.device_model);
  EXPECT_EQ("twirlchange", info.serial_number);
  EXPECT_EQ(enterprise_reporting_private::SETTING_VALUE_ENABLED,
            info.screen_lock_secured);
  EXPECT_EQ(enterprise_reporting_private::SETTING_VALUE_DISABLED,
            info.disk_encrypted);
  ASSERT_EQ(1u, info.mac_addresses.size());
  EXPECT_EQ("00:00:00:00:00:00", info.mac_addresses[0]);
#endif
}

TEST_F(EnterpriseReportingPrivateGetDeviceInfoTest, GetDeviceInfoConversion) {
  // Verify that the conversion from a DeviceInfoFetcher result works,
  // regardless of platform.
  auto device_info_fetcher =
      enterprise_signals::DeviceInfoFetcher::CreateStubInstanceForTesting();

  enterprise_reporting_private::DeviceInfo info =
      EnterpriseReportingPrivateGetDeviceInfoFunction::ToDeviceInfo(
          device_info_fetcher->Fetch());
  EXPECT_EQ("stubOS", info.os_name);
  EXPECT_EQ("0.0.0.0", info.os_version);
  EXPECT_EQ("midnightshift", info.device_host_name);
  EXPECT_EQ("topshot", info.device_model);
  EXPECT_EQ("twirlchange", info.serial_number);
  EXPECT_EQ(enterprise_reporting_private::SETTING_VALUE_ENABLED,
            info.screen_lock_secured);
  EXPECT_EQ(enterprise_reporting_private::SETTING_VALUE_DISABLED,
            info.disk_encrypted);
  ASSERT_EQ(1u, info.mac_addresses.size());
  EXPECT_EQ("00:00:00:00:00:00", info.mac_addresses[0]);
}

#endif  // !defined(OS_CHROMEOS)

class EnterpriseReportingPrivateGetContextInfoTest
    : public ExtensionApiUnittest {
 public:
  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    // Only used to set the right default BuiltInDnsClientEnabled preference
    // value according to the OS. DnsClient and DoH default preferences are
    // updated when the object is created, making the object unnecessary outside
    // this scope.
    StubResolverConfigReader stub_resolver_config_reader(
        g_browser_process->local_state());
  }

  enterprise_reporting_private::ContextInfo GetContextInfo() {
    auto function = base::MakeRefCounted<
        EnterpriseReportingPrivateGetContextInfoFunction>();
    std::unique_ptr<base::Value> context_info_value =
        RunFunctionAndReturnValue(function.get(), "[]");
    EXPECT_TRUE(context_info_value.get());

    enterprise_reporting_private::ContextInfo info;
    EXPECT_TRUE(enterprise_reporting_private::ContextInfo::Populate(
        *context_info_value, &info));

    return info;
  }

  bool BuiltInDnsClientPlatformDefault() {
#if defined(OS_CHROMEOS) || defined(OS_MAC) || defined(OS_ANDROID)
    return true;
#else
    return false;
#endif
  }

  void ExpectDefaultChromeCleanupEnabled(
      enterprise_reporting_private::ContextInfo& info) {
#if defined(OS_WIN)
    EXPECT_TRUE(*info.chrome_cleanup_enabled);
#else
    EXPECT_EQ(nullptr, info.chrome_cleanup_enabled.get());
#endif
  }
};

TEST_F(EnterpriseReportingPrivateGetContextInfoTest, NoSpecialContext) {
  // This tests the data returned by the API is correct when no special context
  // is present, ie no policies are set, the browser is unamanaged, etc.
  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
            info.safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PASSWORD_PROTECTION_TRIGGER_POLICY_UNSET,
      info.password_protection_warning_trigger);
  ExpectDefaultChromeCleanupEnabled(info);
  EXPECT_FALSE(info.chrome_remote_desktop_app_blocked);
}

class EnterpriseReportingPrivateGetContextInfoSafeBrowsingTest
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {};

TEST_P(EnterpriseReportingPrivateGetContextInfoSafeBrowsingTest, Test) {
  std::tuple<bool, bool> params = GetParam();

  bool safe_browsing_enabled = std::get<0>(params);
  bool safe_browsing_enhanced_enabled = std::get<1>(params);

  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                    safe_browsing_enabled);
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced,
                                    safe_browsing_enhanced_enabled);
  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);

  if (safe_browsing_enabled) {
    if (safe_browsing_enhanced_enabled)
      EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_ENHANCED,
                info.safe_browsing_protection_level);
    else
      EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
                info.safe_browsing_protection_level);
  } else {
    EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_DISABLED,
              info.safe_browsing_protection_level);
  }
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PASSWORD_PROTECTION_TRIGGER_POLICY_UNSET,
      info.password_protection_warning_trigger);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextInfoSafeBrowsingTest,
    testing::Values(std::make_tuple(false, false),
                    std::make_tuple(true, false),
                    std::make_tuple(true, true)));

class EnterpriseReportingPrivateGetContextInfoBuiltInDnsClientTest
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<bool> {};

TEST_P(EnterpriseReportingPrivateGetContextInfoBuiltInDnsClientTest, Test) {
  bool policyValue = GetParam();

  g_browser_process->local_state()->SetBoolean(prefs::kBuiltInDnsClientEnabled,
                                               policyValue);

  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
            info.safe_browsing_protection_level);
  EXPECT_EQ(policyValue, info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PASSWORD_PROTECTION_TRIGGER_POLICY_UNSET,
      info.password_protection_warning_trigger);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextInfoBuiltInDnsClientTest,
    testing::Bool());

class EnterpriseReportingPrivateGetContextPasswordProtectionWarningTrigger
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<
          enterprise_reporting_private::PasswordProtectionTrigger> {
 public:
  safe_browsing::PasswordProtectionTrigger MapPasswordProtectionTriggerToPolicy(
      enterprise_reporting_private::PasswordProtectionTrigger enumValue) {
    switch (enumValue) {
      case enterprise_reporting_private::
          PASSWORD_PROTECTION_TRIGGER_PASSWORD_PROTECTION_OFF:
        return safe_browsing::PASSWORD_PROTECTION_OFF;
      case enterprise_reporting_private::
          PASSWORD_PROTECTION_TRIGGER_PASSWORD_REUSE:
        return safe_browsing::PASSWORD_REUSE;
      case enterprise_reporting_private::
          PASSWORD_PROTECTION_TRIGGER_PHISHING_REUSE:
        return safe_browsing::PHISHING_REUSE;
      default:
        NOTREACHED();
        return safe_browsing::PASSWORD_PROTECTION_TRIGGER_MAX;
    }
  }
};

TEST_P(EnterpriseReportingPrivateGetContextPasswordProtectionWarningTrigger,
       Test) {
  enterprise_reporting_private::PasswordProtectionTrigger passwordTriggerValue =
      GetParam();

  profile()->GetPrefs()->SetInteger(
      prefs::kPasswordProtectionWarningTrigger,
      MapPasswordProtectionTriggerToPolicy(passwordTriggerValue));
  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
            info.safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  EXPECT_EQ(passwordTriggerValue, info.password_protection_warning_trigger);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextPasswordProtectionWarningTrigger,
    testing::Values(enterprise_reporting_private::
                        PASSWORD_PROTECTION_TRIGGER_PASSWORD_PROTECTION_OFF,
                    enterprise_reporting_private::
                        PASSWORD_PROTECTION_TRIGGER_PASSWORD_REUSE,
                    enterprise_reporting_private::
                        PASSWORD_PROTECTION_TRIGGER_PHISHING_REUSE));

#if defined(OS_WIN)
class EnterpriseReportingPrivateGetContextInfoChromeCleanupTest
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<bool> {};

TEST_P(EnterpriseReportingPrivateGetContextInfoChromeCleanupTest, Test) {
  bool policyValue = GetParam();

  g_browser_process->local_state()->SetBoolean(prefs::kSwReporterEnabled,
                                               policyValue);

  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
            info.realtime_url_check_mode);
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
            info.safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PASSWORD_PROTECTION_TRIGGER_POLICY_UNSET,
      info.password_protection_warning_trigger);
  EXPECT_EQ(policyValue, *info.chrome_cleanup_enabled);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextInfoChromeCleanupTest,
    testing::Bool());
#endif  // defined(OS_WIN)

class EnterpriseReportingPrivateGetContextInfoChromeRemoteDesktopAppBlockedTest
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<const char*> {
 public:
  void SetURLBlockedPolicy(const std::string& url) {
    base::Value blockList(base::Value::Type::LIST);
    blockList.Append(base::Value(url));

    profile()->GetPrefs()->Set(policy::policy_prefs::kUrlBlocklist,
                               std::move(blockList));
  }
  void SetURLAllowedPolicy(const std::string& url) {
    base::Value allowList(base::Value::Type::LIST);
    allowList.Append(base::Value(url));

    profile()->GetPrefs()->Set(policy::policy_prefs::kUrlAllowlist,
                               std::move(allowList));
  }

  void ExpectDefaultPolicies(enterprise_reporting_private::ContextInfo& info) {
    EXPECT_TRUE(info.browser_affiliation_ids.empty());
    EXPECT_TRUE(info.profile_affiliation_ids.empty());
    EXPECT_TRUE(info.on_file_attached_providers.empty());
    EXPECT_TRUE(info.on_file_downloaded_providers.empty());
    EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
    EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
              info.realtime_url_check_mode);
    EXPECT_TRUE(info.on_security_event_providers.empty());
    EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
    EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
              info.safe_browsing_protection_level);
    EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
              info.built_in_dns_client_enabled);
    EXPECT_EQ(
        enterprise_reporting_private::PASSWORD_PROTECTION_TRIGGER_POLICY_UNSET,
        info.password_protection_warning_trigger);
    ExpectDefaultChromeCleanupEnabled(info);
  }
};

TEST_P(
    EnterpriseReportingPrivateGetContextInfoChromeRemoteDesktopAppBlockedTest,
    BlockedURL) {
  SetURLBlockedPolicy(GetParam());

  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  ExpectDefaultPolicies(info);
  EXPECT_TRUE(info.chrome_remote_desktop_app_blocked);
}

TEST_P(
    EnterpriseReportingPrivateGetContextInfoChromeRemoteDesktopAppBlockedTest,
    AllowedURL) {
  SetURLAllowedPolicy(GetParam());

  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  ExpectDefaultPolicies(info);
  EXPECT_FALSE(info.chrome_remote_desktop_app_blocked);
}

TEST_P(
    EnterpriseReportingPrivateGetContextInfoChromeRemoteDesktopAppBlockedTest,
    BlockedAndAllowedURL) {
  SetURLBlockedPolicy(GetParam());
  SetURLAllowedPolicy(GetParam());

  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  ExpectDefaultPolicies(info);
  EXPECT_FALSE(info.chrome_remote_desktop_app_blocked);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextInfoChromeRemoteDesktopAppBlockedTest,
    testing::Values("https://remotedesktop.google.com",
                    "https://remotedesktop.corp.google.com",
                    "corp.google.com",
                    "google.com",
                    "https://*"));

class EnterpriseReportingPrivateGetContextInfoRealTimeURLCheckTest
    : public EnterpriseReportingPrivateGetContextInfoTest,
      public testing::WithParamInterface<bool> {
 public:
  EnterpriseReportingPrivateGetContextInfoRealTimeURLCheckTest() {
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting("fake-token"));
  }

  bool url_check_enabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    EnterpriseReportingPrivateGetContextInfoRealTimeURLCheckTest,
    testing::Bool());

TEST_P(EnterpriseReportingPrivateGetContextInfoRealTimeURLCheckTest, Test) {
  profile()->GetPrefs()->SetInteger(
      prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode,
      url_check_enabled() ? safe_browsing::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED
                          : safe_browsing::REAL_TIME_CHECK_DISABLED);
  profile()->GetPrefs()->SetInteger(
      prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);
  enterprise_reporting_private::ContextInfo info = GetContextInfo();

  if (url_check_enabled()) {
    EXPECT_EQ(enterprise_reporting_private::
                  REALTIME_URL_CHECK_MODE_ENABLED_MAIN_FRAME,
              info.realtime_url_check_mode);
  } else {
    EXPECT_EQ(enterprise_reporting_private::REALTIME_URL_CHECK_MODE_DISABLED,
              info.realtime_url_check_mode);
  }

  EXPECT_TRUE(info.browser_affiliation_ids.empty());
  EXPECT_TRUE(info.profile_affiliation_ids.empty());
  EXPECT_TRUE(info.on_file_attached_providers.empty());
  EXPECT_TRUE(info.on_file_downloaded_providers.empty());
  EXPECT_TRUE(info.on_bulk_data_entry_providers.empty());
  EXPECT_TRUE(info.on_security_event_providers.empty());
  EXPECT_EQ(version_info::GetVersionNumber(), info.browser_version);
  EXPECT_EQ(enterprise_reporting_private::SAFE_BROWSING_LEVEL_STANDARD,
            info.safe_browsing_protection_level);
  EXPECT_EQ(BuiltInDnsClientPlatformDefault(),
            info.built_in_dns_client_enabled);
  EXPECT_EQ(
      enterprise_reporting_private::PASSWORD_PROTECTION_TRIGGER_POLICY_UNSET,
      info.password_protection_warning_trigger);
}

}  // namespace extensions
