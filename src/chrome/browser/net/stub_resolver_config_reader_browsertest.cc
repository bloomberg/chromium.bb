// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/stub_resolver_config_reader.h"

#include <string>

#include "base/enterprise_util.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/dns/public/secure_dns_mode.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

// TODO(ericorth@chromium.org): Consider validating that the expected
// configuration makes it all the way to the net::HostResolverManager in the
// network service, rather than just testing StubResolverConfigReader output.

namespace {

// A custom matcher to validate a DnsOverHttpsServerConfig instance.
MATCHER_P2(DnsOverHttpsServerConfigMatcher, server_template, use_post, "") {
  return testing::ExplainMatchResult(
      testing::AllOf(
          testing::Field(&net::DnsOverHttpsServerConfig::server_template,
                         server_template),
          testing::Field(&net::DnsOverHttpsServerConfig::use_post, use_post)),
      arg, result_listener);
}

class StubResolverConfigReaderBrowsertest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  StubResolverConfigReaderBrowsertest() {
    scoped_feature_list_.InitWithFeatureState(features::kAsyncDns, GetParam());
  }
  ~StubResolverConfigReaderBrowsertest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    // Normal boilerplate to setup a MockConfigurationPolicyProvider.
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetUpOnMainThread() override {
    // Set the mocked policy provider to act as if no policies are in use by
    // updating to the initial still-empty `policy_map_`.
    ASSERT_TRUE(policy_map_.empty());
    policy_provider_.UpdateChromePolicy(policy_map_);

    config_reader_ = SystemNetworkContextManager::GetStubResolverConfigReader();

    // Only affects future parental controls checks, but that is all that
    // matters here because these tests only deal with checking fresh config
    // reads, not what has been sent to Network Service.
    //
    // TODO(ericorth@chromium.org): If validation of network service state is
    // ever added, need to ensure the result of this override gets sent first.
    config_reader_->OverrideParentalControlsForTesting(
        /*parental_controls_override=*/false);
  }

 protected:
  void SetSecureDnsModePolicy(std::string mode_str) {
    policy_map_.Set(
        policy::key::kDnsOverHttpsMode, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
        base::Value(std::move(mode_str)),
        /*external_data_fetcher=*/nullptr);
    policy_provider_.UpdateChromePolicy(policy_map_);
  }

  void SetDohTemplatesPolicy(std::string teplates_str) {
    policy_map_.Set(
        policy::key::kDnsOverHttpsTemplates, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
        base::Value(std::move(teplates_str)),
        /*external_data_fetcher=*/nullptr);
    policy_provider_.UpdateChromePolicy(policy_map_);
  }

  void ClearPolicies() {
    policy_map_.Clear();
    policy_provider_.UpdateChromePolicy(policy_map_);
  }

  policy::PolicyMap policy_map_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

  raw_ptr<StubResolverConfigReader> config_reader_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Set various DoH modes and DoH template strings and make sure the settings are
// respected.
IN_PROC_BROWSER_TEST_P(StubResolverConfigReaderBrowsertest, ConfigFromPrefs) {
  bool async_dns_feature_enabled = GetParam();

  // Mark as not enterprise managed.
#if defined(OS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(false);
  EXPECT_FALSE(base::IsMachineExternallyManaged());
#endif

  std::string good_post_template = "https://foo.test/";
  std::string good_get_template = "https://bar.test/dns-query{?dns}";
  std::string bad_template = "dns-query{?dns}";
  std::string good_then_bad_template = good_get_template + " " + bad_template;
  std::string bad_then_good_template = bad_template + " " + good_get_template;
  std::string multiple_good_templates =
      "  " + good_get_template + "   " + good_post_template + "  ";

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeSecure);
  local_state->SetString(prefs::kDnsOverHttpsTemplates, bad_template);
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kSecure, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());

  local_state->SetString(prefs::kDnsOverHttpsTemplates, good_post_template);
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kSecure, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(),
              testing::ElementsAreArray({
                  DnsOverHttpsServerConfigMatcher(good_post_template, true),
              }));

  local_state->SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeAutomatic);
  local_state->SetString(prefs::kDnsOverHttpsTemplates, bad_template);
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());

  local_state->SetString(prefs::kDnsOverHttpsTemplates, good_then_bad_template);
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(),
              testing::ElementsAreArray({
                  DnsOverHttpsServerConfigMatcher(good_get_template, false),
              }));

  local_state->SetString(prefs::kDnsOverHttpsTemplates, bad_then_good_template);
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(),
              testing::ElementsAreArray({
                  DnsOverHttpsServerConfigMatcher(good_get_template, false),
              }));

  local_state->SetString(prefs::kDnsOverHttpsTemplates,
                         multiple_good_templates);
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(),
              testing::ElementsAreArray({
                  DnsOverHttpsServerConfigMatcher(good_get_template, false),
                  DnsOverHttpsServerConfigMatcher(good_post_template, true),
              }));

  local_state->SetString(prefs::kDnsOverHttpsMode, SecureDnsConfig::kModeOff);
  local_state->SetString(prefs::kDnsOverHttpsTemplates, good_get_template);
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());

  local_state->SetString(prefs::kDnsOverHttpsMode, "no_match");
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());

  // Test case with policy BuiltInDnsClientEnabled enabled. The DoH fields
  // should be unaffected.
  local_state->Set(prefs::kBuiltInDnsClientEnabled,
                   base::Value(!async_dns_feature_enabled));
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(!async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());
}

// Set various policies and ensure the correct prefs.
IN_PROC_BROWSER_TEST_P(StubResolverConfigReaderBrowsertest, ConfigFromPolicy) {
  bool async_dns_feature_enabled = GetParam();

// Mark as not enterprise managed.
#if defined(OS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(false);
  EXPECT_FALSE(base::IsMachineExternallyManaged());
#endif

  // Start with default non-set policies.
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/false);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  if (base::FeatureList::IsEnabled(features::kDnsOverHttps)) {
    EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kAutomatic);
  } else {
    EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kOff);
  }
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());

  // ChromeOS includes its own special functionality to set default policies if
  // any policies are set.  This function is not declared and cannot be invoked
  // in non-CrOS builds. Expect these enterprise user defaults to disable DoH.
#if defined(OS_CHROMEOS)
  // Applies the special ChromeOS defaults to `policy_map_`.
  policy::SetEnterpriseUsersDefaults(&policy_map_);
  // Send the PolicyMap to the mock policy provider.
  policy_provider_.UpdateChromePolicy(policy_map_);
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/false);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kOff);
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());
#endif  // defined(OS_CHROMEOS)

  // Disable DoH by policy
  ClearPolicies();
  SetSecureDnsModePolicy("off");
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/false);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kOff);
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());

  // Automatic mode by policy
  SetSecureDnsModePolicy("automatic");
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/false);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kAutomatic);
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());

  // Secure mode by policy
  SetSecureDnsModePolicy("secure");
  SetDohTemplatesPolicy("https://doh.test/");
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/false);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kSecure);
  EXPECT_THAT(secure_dns_config.servers(),
              testing::ElementsAre(DnsOverHttpsServerConfigMatcher(
                  "https://doh.test/", /*use_post=*/true)));

  // Invalid template policy
  SetSecureDnsModePolicy("secure");
  SetDohTemplatesPolicy("invalid template");
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/false);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kSecure);
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());

  // Invalid mode policy
  SetSecureDnsModePolicy("invalid");
  SetDohTemplatesPolicy("https://doh.test/");
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/false);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kOff);
  // Expect empty templates if mode policy is invalid.
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());
}

// Test that parental controls detection interacts correctly with prefs and
// policies.
IN_PROC_BROWSER_TEST_P(StubResolverConfigReaderBrowsertest,
                       ConfigFromParentalControls) {
// Mark as not enterprise managed.
#if defined(OS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(false);
  EXPECT_FALSE(base::IsMachineExternallyManaged());
#endif

  config_reader_->OverrideParentalControlsForTesting(
      /*parental_controls_override=*/true);

  // Parental controls takes precedence over regular prefs.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeAutomatic);
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/true);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kOff);
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());

  // Policy takes precedence over parental controls.
  SetSecureDnsModePolicy("automatic");
  SetDohTemplatesPolicy("");
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/true);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kAutomatic);
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         StubResolverConfigReaderBrowsertest,
                         ::testing::Bool());

}  // namespace
