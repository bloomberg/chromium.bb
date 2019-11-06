// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/managed_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

using namespace policy;

class ManagedUiTest : public InProcessBrowserTest {
 public:
  ManagedUiTest() = default;
  ~ManagedUiTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    BrowserPolicyConnectorBase::SetPolicyProviderForTesting(&provider_);
  }

  MockConfigurationPolicyProvider* provider() { return &provider_; }

 private:
  MockConfigurationPolicyProvider provider_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUiTest);
};

IN_PROC_BROWSER_TEST_F(ManagedUiTest, ShouldDisplayManagedUiFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine("", "ShowManagedUi");

  PolicyMap policy_map;
  policy_map.Set("test-policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                 POLICY_SOURCE_PLATFORM,
                 std::make_unique<base::Value>("hello world"), nullptr);
  provider()->UpdateChromePolicy(policy_map);

  EXPECT_FALSE(chrome::ShouldDisplayManagedUi(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, ShouldDisplayManagedUiNoPolicies) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine("ShowManagedUi", "");

  EXPECT_FALSE(chrome::ShouldDisplayManagedUi(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, ShouldDisplayManagedUiOnDesktop) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine("ShowManagedUi", "");

  PolicyMap policy_map;
  policy_map.Set("test-policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                 POLICY_SOURCE_PLATFORM,
                 std::make_unique<base::Value>("hello world"), nullptr);
  provider()->UpdateChromePolicy(policy_map);

#if defined(OS_CHROMEOS)
  EXPECT_FALSE(chrome::ShouldDisplayManagedUi(browser()->profile()));
#else
  EXPECT_TRUE(chrome::ShouldDisplayManagedUi(browser()->profile()));
#endif
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiMenuItemLabel) {
  TestingProfile::Builder builder;
  auto profile = builder.Build();

  TestingProfile::Builder builder_with_domain;
  builder_with_domain.SetProfileName("foobar@example.com");
  auto profile_with_domain = builder_with_domain.Build();

  EXPECT_EQ(base::UTF8ToUTF16("Managed by your organization"),
            chrome::GetManagedUiMenuItemLabel(profile.get()));
  EXPECT_EQ(base::UTF8ToUTF16("Managed by example.com"),
            chrome::GetManagedUiMenuItemLabel(profile_with_domain.get()));
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiWebUILabel) {
  TestingProfile::Builder builder;
  auto profile = builder.Build();

  TestingProfile::Builder builder_with_domain;
  builder_with_domain.SetProfileName("foobar@example.com");
  auto profile_with_domain = builder_with_domain.Build();

#if !defined(OS_CHROMEOS)
  EXPECT_EQ(
      base::UTF8ToUTF16(
          "Your <a href=\"chrome://management\">browser is managed</a> by your "
          "organization"),
      chrome::GetManagedUiWebUILabel(profile.get()));
  EXPECT_EQ(
      base::UTF8ToUTF16(
          "Your <a href=\"chrome://management\">browser is managed</a> by "
          "example.com"),
      chrome::GetManagedUiWebUILabel(profile_with_domain.get()));
#else
  EXPECT_EQ(
      base::UTF8ToUTF16("Your <a href=\"chrome://management\">Chrome device is "
                        "managed</a> by your organization"),
      chrome::GetManagedUiWebUILabel(profile.get()));
  EXPECT_EQ(
      base::UTF8ToUTF16("Your <a href=\"chrome://management\">Chrome device is "
                        "managed</a> by example.com"),
      chrome::GetManagedUiWebUILabel(profile_with_domain.get()));
#endif
}
