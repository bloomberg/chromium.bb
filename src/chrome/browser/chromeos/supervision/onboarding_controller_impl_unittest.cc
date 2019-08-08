// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/supervision/onboarding_controller_impl.h"

#include <memory>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/supervision/mojom/onboarding_controller.mojom.h"
#include "chrome/browser/chromeos/supervision/onboarding_constants.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace supervision {
namespace {

const char kTestAccountId[] = "test-account-id";

}  // namespace

class FakeOnboardingWebviewHost : mojom::OnboardingWebviewHost {
 public:
  explicit FakeOnboardingWebviewHost(
      mojom::OnboardingWebviewHostRequest request)
      : binding_(this, std::move(request)) {}

  void LoadPage(mojom::OnboardingPagePtr page,
                LoadPageCallback callback) override {
    ASSERT_FALSE(exited_flow_);

    page_loaded_ = *page;

    std::move(callback).Run(custom_header_value_);
  }

  void ExitFlow() override {
    ASSERT_FALSE(exited_flow_);

    exited_flow_ = true;
  }

  bool exited_flow() const { return exited_flow_; }

  const base::Optional<mojom::OnboardingPage>& page_loaded() {
    return page_loaded_;
  }

  void clear_custom_header_value() { custom_header_value_ = base::nullopt; }

  void set_custom_header_value(const std::string& custom_header_value) {
    custom_header_value_ = custom_header_value;
  }

 private:
  mojo::Binding<mojom::OnboardingWebviewHost> binding_;

  bool exited_flow_ = false;
  base::Optional<std::string> custom_header_value_;
  base::Optional<mojom::OnboardingPage> page_loaded_;

  DISALLOW_COPY_AND_ASSIGN(FakeOnboardingWebviewHost);
};

class OnboardingControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    identity_test_env()->MakeAccountAvailable(kTestAccountId);
    identity_test_env()->SetPrimaryAccount(kTestAccountId);

    controller_impl_ = std::make_unique<OnboardingControllerImpl>(profile());

    controller_impl_->BindRequest(mojo::MakeRequest(&controller_));
  }

  void BindWebviewHost() {
    mojom::OnboardingWebviewHostPtr webview_host_proxy;
    webview_host_ = std::make_unique<FakeOnboardingWebviewHost>(
        mojo::MakeRequest(&webview_host_proxy));

    controller_->BindWebviewHost(std::move(webview_host_proxy));
  }

  void WaitForAuthRequestAndReturnError() {
    identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
            GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));

    base::RunLoop().RunUntilIdle();
  }

  void WaitForAuthRequestAndReturnToken(const std::string& access_token) {
    identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            access_token, base::Time::Now() + base::TimeDelta::FromHours(1));

    base::RunLoop().RunUntilIdle();
  }

  Profile* profile() { return profile_.get(); }

  identity::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  FakeOnboardingWebviewHost* webview_host() { return webview_host_.get(); }

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<OnboardingControllerImpl> controller_impl_;
  mojom::OnboardingControllerPtr controller_;
  std::unique_ptr<FakeOnboardingWebviewHost> webview_host_;
};

TEST_F(OnboardingControllerTest, ExitFlowOnAuthError) {
  BindWebviewHost();

  WaitForAuthRequestAndReturnError();

  EXPECT_FALSE(webview_host()->page_loaded().has_value());
  EXPECT_TRUE(webview_host()->exited_flow());
}

TEST_F(OnboardingControllerTest, RequestWebviewHostToLoadStartPageCorrectly) {
  BindWebviewHost();

  WaitForAuthRequestAndReturnToken("fake_access_token");

  ASSERT_TRUE(webview_host()->page_loaded().has_value());

  EXPECT_EQ(webview_host()->page_loaded()->url,
            GURL("https://families.google.com/kids/deviceonboarding/start"));
  EXPECT_EQ(webview_host()->page_loaded()->access_token, "fake_access_token");
  EXPECT_EQ(webview_host()->page_loaded()->custom_header_name,
            kExperimentHeaderName);
  EXPECT_EQ(webview_host()->page_loaded()->url_filter_pattern,
            "https://families.google.com/*");
}

TEST_F(OnboardingControllerTest, OverridePageUrlsByCommandLine) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kSupervisionOnboardingUrlPrefix,
      "https://example.com/");

  BindWebviewHost();

  WaitForAuthRequestAndReturnToken("fake_access_token");

  ASSERT_TRUE(webview_host()->page_loaded().has_value());

  EXPECT_EQ(webview_host()->page_loaded()->url,
            GURL("https://example.com/kids/deviceonboarding/start"));
  EXPECT_EQ(webview_host()->page_loaded()->access_token, "fake_access_token");
  EXPECT_EQ(webview_host()->page_loaded()->custom_header_name,
            kExperimentHeaderName);
  EXPECT_EQ(webview_host()->page_loaded()->url_filter_pattern,
            "https://example.com/*");
}

TEST_F(OnboardingControllerTest,
       StayInFlowWhenHeaderValueIsCorrectAndFlagIsSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kSupervisionOnboardingScreens);
  BindWebviewHost();

  webview_host()->set_custom_header_value(kDeviceOnboardingExperimentName);
  WaitForAuthRequestAndReturnToken("fake_access_token");

  EXPECT_FALSE(webview_host()->exited_flow());
}

TEST_F(OnboardingControllerTest, ExitFlowWhenFlagIsNotSet) {
  BindWebviewHost();

  webview_host()->set_custom_header_value(kDeviceOnboardingExperimentName);
  WaitForAuthRequestAndReturnToken("fake_access_token");

  EXPECT_TRUE(webview_host()->exited_flow());
}

TEST_F(OnboardingControllerTest, ExitFlowWhenHeaderValueIsMissing) {
  BindWebviewHost();

  webview_host()->clear_custom_header_value();
  WaitForAuthRequestAndReturnToken("fake_access_token");

  EXPECT_TRUE(webview_host()->exited_flow());
}

TEST_F(OnboardingControllerTest, ExitFlowWhenHeaderValueIsWrong) {
  BindWebviewHost();

  webview_host()->set_custom_header_value("clearly-wrong-header-value");
  WaitForAuthRequestAndReturnToken("fake_access_token");

  EXPECT_TRUE(webview_host()->exited_flow());
}

TEST_F(OnboardingControllerTest,
       SetEligibleForKioskNextWhenHeaderValueIsCorrect) {
  BindWebviewHost();

  webview_host()->set_custom_header_value(kDeviceOnboardingExperimentName);
  WaitForAuthRequestAndReturnToken("fake_access_token");

  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEligible));
}

TEST_F(OnboardingControllerTest,
       SetNotEligibleForKioskNextWhenHeaderValueIsWrong) {
  BindWebviewHost();

  webview_host()->set_custom_header_value("clearly-wrong-header-value");
  WaitForAuthRequestAndReturnToken("fake_access_token");

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(ash::prefs::kKioskNextShellEligible));
}

}  // namespace supervision
}  // namespace chromeos
