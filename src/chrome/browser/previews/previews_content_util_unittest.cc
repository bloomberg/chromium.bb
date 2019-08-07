// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_content_util.h"

#include <memory>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/previews_state.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {

// A test implementation of PreviewsDecider that simply returns whether the
// preview type feature is enabled (ignores ECT and blacklist considerations).
class PreviewEnabledPreviewsDecider : public PreviewsDecider {
 public:
  PreviewEnabledPreviewsDecider() {}
  ~PreviewEnabledPreviewsDecider() override {}

  bool ShouldAllowPreviewAtNavigationStart(PreviewsUserData* previews_data,
                                           const GURL& url,
                                           bool is_reload,
                                           PreviewsType type) const override {
    return IsEnabled(type);
  }

  bool ShouldCommitPreview(PreviewsUserData* previews_data,
                           const GURL& url,
                           PreviewsType type) const override {
    EXPECT_TRUE(type == PreviewsType::NOSCRIPT ||
                type == PreviewsType::RESOURCE_LOADING_HINTS);
    return IsEnabled(type);
  }

  bool LoadPageHints(const GURL& url) override {
    return url.host_piece().ends_with("hintcachedhost.com");
  }

  bool GetResourceLoadingHints(
      const GURL& url,
      std::vector<std::string>* out_resource_patterns_to_block) const override {
    return false;
  }

  void LogHintCacheMatch(const GURL& url, bool is_committed) const override {}

 private:
  bool IsEnabled(PreviewsType type) const {
    switch (type) {
      case previews::PreviewsType::OFFLINE:
        return params::IsOfflinePreviewsEnabled();
      case previews::PreviewsType::LOFI:
        return params::IsClientLoFiEnabled();
      case previews::PreviewsType::DEPRECATED_AMP_REDIRECTION:
        return false;
      case previews::PreviewsType::NOSCRIPT:
        return params::IsNoScriptPreviewsEnabled();
      case previews::PreviewsType::RESOURCE_LOADING_HINTS:
        return params::IsResourceLoadingHintsEnabled();
      case previews::PreviewsType::LITE_PAGE_REDIRECT:
        return params::IsLitePageServerPreviewsEnabled();
      case PreviewsType::LITE_PAGE:
      case PreviewsType::NONE:
      case PreviewsType::UNSPECIFIED:
      case PreviewsType::LAST:
        break;
    }
    NOTREACHED();
    return false;
  }
};

class PreviewsContentUtilTest : public testing::Test {
 public:
  PreviewsContentUtilTest() {}
  ~PreviewsContentUtilTest() override {}

  PreviewsDecider* enabled_previews_decider() {
    return &enabled_previews_decider_;
  }

 protected:
  base::test::ScopedTaskEnvironment task_environment_;

 private:
  PreviewEnabledPreviewsDecider enabled_previews_decider_;
};

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStatePreviewsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "ClientLoFi,ResourceLoadingHints,NoScriptPreviews" /* enable_features */,
      "Previews" /* disable_features */);
  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool is_redirect = false;
  bool is_data_saver_user = true;
  EXPECT_EQ(
      content::PREVIEWS_UNSPECIFIED,
      previews::DetermineAllowedClientPreviewsState(
          &user_data, GURL("http://www.google.com"), is_reload, is_redirect,
          is_data_saver_user, enabled_previews_decider(), nullptr));
  EXPECT_EQ(
      content::PREVIEWS_UNSPECIFIED,
      previews::DetermineAllowedClientPreviewsState(
          &user_data, GURL("http://www.google.com"), is_reload, is_redirect,
          is_data_saver_user, enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateDataSaverDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews,ClientLoFi,ResourceLoadingHints,NoScriptPreviews",
      {} /* disable_features */);
  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool is_redirect = false;
  bool is_data_saver_user = true;
  EXPECT_EQ(
      content::OFFLINE_PAGE_ON | content::CLIENT_LOFI_ON |
          content::RESOURCE_LOADING_HINTS_ON | content::NOSCRIPT_ON,
      previews::DetermineAllowedClientPreviewsState(
          &user_data, GURL("http://www.google.com"), is_reload, is_redirect,
          is_data_saver_user, enabled_previews_decider(), nullptr));
  is_data_saver_user = false;
  EXPECT_EQ(
      content::PREVIEWS_UNSPECIFIED,
      previews::DetermineAllowedClientPreviewsState(
          &user_data, GURL("http://www.google.com"), is_reload, is_redirect,
          is_data_saver_user, enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateOfflineAndRedirects) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews", "ClientLoFi,ResourceLoadingHints,NoScriptPreviews");
  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool is_redirect = false;
  bool is_data_saver_user = true;
  EXPECT_EQ(
      content::OFFLINE_PAGE_ON,
      previews::DetermineAllowedClientPreviewsState(
          &user_data, GURL("http://www.google.com"), is_reload, is_redirect,
          is_data_saver_user, enabled_previews_decider(), nullptr));
  EXPECT_FALSE(user_data.is_redirect());
  user_data.set_allowed_previews_state(content::OFFLINE_PAGE_ON);
  is_redirect = true;
  EXPECT_EQ(
      content::OFFLINE_PAGE_ON,
      previews::DetermineAllowedClientPreviewsState(
          &user_data, GURL("http://www.google.com"), is_reload, is_redirect,
          is_data_saver_user, enabled_previews_decider(), nullptr));
  EXPECT_TRUE(user_data.is_redirect());
  user_data.set_allowed_previews_state(content::PREVIEWS_OFF);
  EXPECT_EQ(
      content::PREVIEWS_UNSPECIFIED,
      previews::DetermineAllowedClientPreviewsState(
          &user_data, GURL("http://www.google.com"), is_reload, is_redirect,
          is_data_saver_user, enabled_previews_decider(), nullptr));
  is_redirect = false;
  EXPECT_EQ(
      content::OFFLINE_PAGE_ON,
      previews::DetermineAllowedClientPreviewsState(
          &user_data, GURL("http://www.google.com"), is_reload, is_redirect,
          is_data_saver_user, enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest, DetermineAllowedClientPreviewsStateClientLoFi) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("Previews,ClientLoFi", std::string());
  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool is_redirect = false;
  bool is_data_saver_user = true;
  EXPECT_TRUE(content::CLIENT_LOFI_ON &
              previews::DetermineAllowedClientPreviewsState(
                  &user_data, GURL("https://www.google.com"), is_reload,
                  is_redirect, is_data_saver_user, enabled_previews_decider(),
                  nullptr));
  EXPECT_TRUE(content::CLIENT_LOFI_ON &
              previews::DetermineAllowedClientPreviewsState(
                  &user_data, GURL("http://www.google.com"), is_reload,
                  is_redirect, is_data_saver_user, enabled_previews_decider(),
                  nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateResourceLoadingHints) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("Previews,ResourceLoadingHints",
                                          std::string());
  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool is_redirect = false;
  bool is_data_saver_user = true;
  EXPECT_LT(0, content::RESOURCE_LOADING_HINTS_ON &
                   previews::DetermineAllowedClientPreviewsState(
                       &user_data, GURL("https://www.google.com"), is_reload,
                       is_redirect, is_data_saver_user,
                       enabled_previews_decider(), nullptr));
  EXPECT_LT(0, content::RESOURCE_LOADING_HINTS_ON &
                   previews::DetermineAllowedClientPreviewsState(
                       &user_data, GURL("http://www.google.com"), is_reload,
                       is_redirect, is_data_saver_user,
                       enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateNoScriptAndClientLoFi) {
  // Enable both Client LoFi and NoScript.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews,ClientLoFi,NoScriptPreviews", std::string());

  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool is_redirect = false;
  bool is_data_saver_user = true;
  // Verify both are enabled.
  EXPECT_TRUE((content::NOSCRIPT_ON | content::CLIENT_LOFI_ON) &
              previews::DetermineAllowedClientPreviewsState(
                  &user_data, GURL("https://www.google.com"), is_reload,
                  is_redirect, is_data_saver_user, enabled_previews_decider(),
                  nullptr));
  EXPECT_TRUE((content::NOSCRIPT_ON | content::CLIENT_LOFI_ON) &
              previews::DetermineAllowedClientPreviewsState(
                  &user_data, GURL("http://www.google.com"), is_reload,
                  is_redirect, is_data_saver_user, enabled_previews_decider(),
                  nullptr));

  // Verify non-HTTP[S] URL has no previews enabled.
  EXPECT_EQ(content::PREVIEWS_UNSPECIFIED,
            previews::DetermineAllowedClientPreviewsState(
                &user_data, GURL("data://someblob"), is_reload, is_redirect,
                is_data_saver_user, enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateLitePageRedirect) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("Previews,LitePageServerPreviews",
                                          std::string());

  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool is_redirect = false;
  bool is_data_saver_user = true;
  // Verify preview is enabled on HTTPS.
  EXPECT_TRUE(content::LITE_PAGE_REDIRECT_ON &
              previews::DetermineAllowedClientPreviewsState(
                  &user_data, GURL("https://www.google.com"), is_reload,
                  is_redirect, is_data_saver_user, enabled_previews_decider(),
                  nullptr));

  // Verify non-HTTP[S] URL has no previews enabled.
  EXPECT_EQ(content::PREVIEWS_UNSPECIFIED,
            previews::DetermineAllowedClientPreviewsState(
                &user_data, GURL("data://someblob"), is_reload, is_redirect,
                is_data_saver_user, enabled_previews_decider(), nullptr));

  // Other checks are performed in browser tests due to the nature of needing
  // fully initialized browser state.
}

TEST_F(PreviewsContentUtilTest,
       DetermineAllowedClientPreviewsStateLitePageRedirectAndPageHintPreviews) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews,LitePageServerPreviews,ResourceLoadingHints,NoScriptPreviews",
      std::string());

  PreviewsUserData user_data(1);
  bool is_reload = false;
  bool is_redirect = false;
  bool is_data_saver_user = true;
  // Verify Lite Page Redirect enabled for host without page hints.
  content::PreviewsState ps1 = previews::DetermineAllowedClientPreviewsState(
      &user_data, GURL("https://www.google.com"), is_reload, is_redirect,
      is_data_saver_user, enabled_previews_decider(), nullptr);
  EXPECT_TRUE(ps1 & content::LITE_PAGE_REDIRECT_ON);
  EXPECT_TRUE(ps1 & content::RESOURCE_LOADING_HINTS_ON);
  EXPECT_TRUE(ps1 & content::NOSCRIPT_ON);

  // Verify only page hint client previews enabled with known page hints.
  content::PreviewsState ps2 = previews::DetermineAllowedClientPreviewsState(
      &user_data, GURL("https://www.hintcachedhost.com"), is_reload,
      is_redirect, is_data_saver_user, enabled_previews_decider(), nullptr);
  EXPECT_FALSE(ps2 & content::LITE_PAGE_REDIRECT_ON);
  EXPECT_TRUE(ps2 & content::RESOURCE_LOADING_HINTS_ON);
  EXPECT_TRUE(ps2 & content::NOSCRIPT_ON);

  {
    // Now set parameter to override page hints.
    std::map<std::string, std::string> parameters;
    parameters["override_pagehints"] = "true";
    base::test::ScopedFeatureList nested_feature_list;
    nested_feature_list.InitAndEnableFeatureWithParameters(
        features::kLitePageServerPreviews, parameters);

    // Verify Lite Page Redirect now enabled for host with page hints.
    content::PreviewsState ps = previews::DetermineAllowedClientPreviewsState(
        &user_data, GURL("https://www.hintcachedhost.com"), is_reload,
        is_redirect, is_data_saver_user, enabled_previews_decider(), nullptr);
    EXPECT_TRUE(ps & content::LITE_PAGE_REDIRECT_ON);
    EXPECT_TRUE(ps & content::RESOURCE_LOADING_HINTS_ON);
    EXPECT_TRUE(ps & content::NOSCRIPT_ON);
  }
}

TEST_F(PreviewsContentUtilTest, DetermineCommittedClientPreviewsState) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "Previews,ClientLoFi,NoScriptPreviews,ResourceLoadingHints",
      std::string());
  PreviewsUserData user_data(1);
  user_data.set_navigation_ect(net::EFFECTIVE_CONNECTION_TYPE_2G);
  base::HistogramTester histogram_tester;

  // Server bits take precedence over NoScript:
  EXPECT_EQ(content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                content::CLIENT_LOFI_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                    content::CLIENT_LOFI_ON | content::NOSCRIPT_ON,
                enabled_previews_decider(), nullptr));
  histogram_tester.ExpectUniqueSample(
      "Previews.Triggered.EffectiveConnectionType2.LitePage",
      static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_2G), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.Triggered.EffectiveConnectionType2", 1);

  content::PreviewsState lite_page_redirect_enabled =
      content::CLIENT_LOFI_ON | content::NOSCRIPT_ON |
      content::RESOURCE_LOADING_HINTS_ON | content::LITE_PAGE_REDIRECT_ON;

  // LITE_PAGE_REDIRECT takes precedence over NoScript, Resource Loading Hints,
  // and Client LoFi when the committed URL is for the lite page previews
  // server.
  EXPECT_EQ(
      content::LITE_PAGE_REDIRECT_ON,
      previews::DetermineCommittedClientPreviewsState(
          &user_data, GURL("https://litepages.googlezip.net/?u=google.com"),
          lite_page_redirect_enabled, enabled_previews_decider(), nullptr));
  histogram_tester.ExpectUniqueSample(
      "Previews.Triggered.EffectiveConnectionType2.LitePageRedirect",
      static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_2G), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.Triggered.EffectiveConnectionType2", 2);

  // Verify LITE_PAGE_REDIRECT_ON not committed for non-lite-page-sever URL.
  EXPECT_NE(
      content::LITE_PAGE_REDIRECT_ON,
      previews::DetermineCommittedClientPreviewsState(
          &user_data, GURL("https://www.google.com"),
          lite_page_redirect_enabled, enabled_previews_decider(), nullptr));
  histogram_tester.ExpectUniqueSample(
      "Previews.Triggered.EffectiveConnectionType2.ResourceLoadingHints",
      static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_2G), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.Triggered.EffectiveConnectionType2", 3);

  // NoScript has precedence over Client LoFi - kept for committed HTTPS:
  EXPECT_EQ(content::NOSCRIPT_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::CLIENT_LOFI_ON | content::NOSCRIPT_ON,
                enabled_previews_decider(), nullptr));
  histogram_tester.ExpectUniqueSample(
      "Previews.Triggered.EffectiveConnectionType2.NoScript",
      static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_2G), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.Triggered.EffectiveConnectionType2", 4);

  // Try different navigation ECT value.
  user_data.set_navigation_ect(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  // RESOURCE_LOADING_HINTS has precedence over Client LoFi and NoScript.
  EXPECT_EQ(content::RESOURCE_LOADING_HINTS_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::CLIENT_LOFI_ON | content::NOSCRIPT_ON |
                    content::RESOURCE_LOADING_HINTS_ON,
                enabled_previews_decider(), nullptr));
  histogram_tester.ExpectBucketCount(
      "Previews.Triggered.EffectiveConnectionType2.ResourceLoadingHints",
      static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.Triggered.EffectiveConnectionType2", 5);

  // NoScript has precedence over Client LoFi - except for committed HTTP:
  EXPECT_EQ(content::CLIENT_LOFI_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("http://www.google.com"),
                content::CLIENT_LOFI_ON | content::NOSCRIPT_ON |
                    content::RESOURCE_LOADING_HINTS_ON,
                enabled_previews_decider(), nullptr));
  histogram_tester.ExpectUniqueSample(
      "Previews.Triggered.EffectiveConnectionType2.LoFi",
      static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G), 1);
  histogram_tester.ExpectTotalCount(
      "Previews.Triggered.EffectiveConnectionType2", 6);

  // Only Client LoFi:
  EXPECT_EQ(content::CLIENT_LOFI_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::CLIENT_LOFI_ON, enabled_previews_decider(), nullptr));
  histogram_tester.ExpectUniqueSample(
      "Previews.Triggered.EffectiveConnectionType2.LoFi",
      static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G), 2);
  histogram_tester.ExpectTotalCount(
      "Previews.Triggered.EffectiveConnectionType2", 7);

  // Only NoScript:
  EXPECT_EQ(content::NOSCRIPT_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::NOSCRIPT_ON, enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest,
       DetermineCommittedClientPreviewsStateNoScriptCheckIfStillAllowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("Previews,ClientLoFi",
                                          "NoScriptPreviews");
  PreviewsUserData user_data(1);
  // NoScript not allowed at commit time so Client LoFi chosen:
  EXPECT_EQ(content::CLIENT_LOFI_ON,
            previews::DetermineCommittedClientPreviewsState(
                &user_data, GURL("https://www.google.com"),
                content::CLIENT_LOFI_ON | content::NOSCRIPT_ON,
                enabled_previews_decider(), nullptr));
}

TEST_F(PreviewsContentUtilTest, GetMainFramePreviewsType) {
  // Simple cases:
  EXPECT_EQ(previews::PreviewsType::LITE_PAGE,
            previews::GetMainFramePreviewsType(content::SERVER_LITE_PAGE_ON));
  EXPECT_EQ(previews::PreviewsType::LOFI,
            previews::GetMainFramePreviewsType(content::SERVER_LOFI_ON));
  EXPECT_EQ(previews::PreviewsType::NOSCRIPT,
            previews::GetMainFramePreviewsType(content::NOSCRIPT_ON));
  EXPECT_EQ(
      previews::PreviewsType::RESOURCE_LOADING_HINTS,
      previews::GetMainFramePreviewsType(content::RESOURCE_LOADING_HINTS_ON));
  EXPECT_EQ(previews::PreviewsType::LOFI,
            previews::GetMainFramePreviewsType(content::CLIENT_LOFI_ON));
  EXPECT_EQ(previews::PreviewsType::LITE_PAGE_REDIRECT,
            previews::GetMainFramePreviewsType(content::LITE_PAGE_REDIRECT_ON));

  // NONE cases:
  EXPECT_EQ(previews::PreviewsType::NONE,
            previews::GetMainFramePreviewsType(content::PREVIEWS_UNSPECIFIED));
  EXPECT_EQ(previews::PreviewsType::NONE,
            previews::GetMainFramePreviewsType(content::PREVIEWS_NO_TRANSFORM));

  // Precedence cases when server preview is available:
  EXPECT_EQ(previews::PreviewsType::LITE_PAGE,
            previews::GetMainFramePreviewsType(
                content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON |
                content::NOSCRIPT_ON | content::CLIENT_LOFI_ON |
                content::RESOURCE_LOADING_HINTS_ON));
  EXPECT_EQ(previews::PreviewsType::LOFI,
            previews::GetMainFramePreviewsType(
                content::SERVER_LOFI_ON | content::NOSCRIPT_ON |
                content::CLIENT_LOFI_ON | content::RESOURCE_LOADING_HINTS_ON));

  // Precedence cases when server preview is not available:
  EXPECT_EQ(previews::PreviewsType::NOSCRIPT,
            previews::GetMainFramePreviewsType(content::NOSCRIPT_ON |
                                               content::CLIENT_LOFI_ON));
  EXPECT_EQ(previews::PreviewsType::RESOURCE_LOADING_HINTS,
            previews::GetMainFramePreviewsType(
                content::NOSCRIPT_ON | content::CLIENT_LOFI_ON |
                content::RESOURCE_LOADING_HINTS_ON));
  EXPECT_EQ(
      previews::PreviewsType::LITE_PAGE_REDIRECT,
      previews::GetMainFramePreviewsType(
          content::NOSCRIPT_ON | content::CLIENT_LOFI_ON |
          content::RESOURCE_LOADING_HINTS_ON | content::LITE_PAGE_REDIRECT_ON));
}

class PreviewsContentSimulatedNavigationTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PreviewsUITabHelper::CreateForWebContents(web_contents());
  }

  previews::PreviewsUserData* GetPreviewsUserData(
      content::NavigationHandle* handle) {
    PreviewsUITabHelper* tab_helper =
        PreviewsUITabHelper::FromWebContents(web_contents());
    return tab_helper->GetPreviewsUserData(handle);
  }

  content::NavigationHandle* StartNavigation() {
    navigation_simulator_ =
        content::NavigationSimulator::CreateBrowserInitiated(
            GURL("https://test.com"), web_contents());
    navigation_simulator_->Start();

    PreviewsUITabHelper* tab_helper =
        PreviewsUITabHelper::FromWebContents(web_contents());
    tab_helper->CreatePreviewsUserDataForNavigationHandle(
        navigation_simulator_->GetNavigationHandle(), 1);

    return navigation_simulator_->GetNavigationHandle();
  }

  content::NavigationHandle* StartNavigationAndReadyCommit() {
    navigation_simulator_ =
        content::NavigationSimulator::CreateBrowserInitiated(
            GURL("https://test.com"), web_contents());
    navigation_simulator_->Start();

    PreviewsUITabHelper* tab_helper =
        PreviewsUITabHelper::FromWebContents(web_contents());
    tab_helper->CreatePreviewsUserDataForNavigationHandle(
        navigation_simulator_->GetNavigationHandle(), 1);

    navigation_simulator_->ReadyToCommit();
    return navigation_simulator_->GetNavigationHandle();
  }

 private:
  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;
};

TEST_F(PreviewsContentSimulatedNavigationTest, TestCoinFlipBeforeCommit) {
  struct TestCase {
    std::string msg;
    bool enable_feature;
    // True maps to previews::CoinFlipHoldbackResult::kHoldback.
    bool set_random_coin_flip_for_navigation;
    bool set_coin_flip_override;
    previews::CoinFlipHoldbackResult want_coin_flip_result;
    content::PreviewsState initial_state;
    content::PreviewsState want_returned;
  };
  const TestCase kTestCases[]{
      {
          .msg = "Feature disabled, no affect, heads",
          .enable_feature = false,
          .set_random_coin_flip_for_navigation = true,
          .set_coin_flip_override = false,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kNotSet,
          .initial_state = content::CLIENT_LOFI_ON,
          .want_returned = content::CLIENT_LOFI_ON,
      },
      {
          .msg = "Feature disabled, no affect, tails",
          .enable_feature = false,
          .set_random_coin_flip_for_navigation = false,
          .set_coin_flip_override = false,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kNotSet,
          .initial_state = content::CLIENT_LOFI_ON,
          .want_returned = content::CLIENT_LOFI_ON,
      },
      {
          .msg = "Feature disabled, no affect, forced override",
          .enable_feature = false,
          .set_random_coin_flip_for_navigation = false,
          .set_coin_flip_override = true,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kNotSet,
          .initial_state = content::CLIENT_LOFI_ON,
          .want_returned = content::CLIENT_LOFI_ON,
      },
      {
          .msg = "After-commit decided previews are not affected before commit "
                 "on true coin flip",
          .enable_feature = true,
          .set_random_coin_flip_for_navigation = true,
          .set_coin_flip_override = false,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kNotSet,
          .initial_state = content::CLIENT_LOFI_ON,
          .want_returned = content::CLIENT_LOFI_ON,
      },
      {
          .msg = "After-commit decided previews are not affected before commit "
                 "on forced override",
          .enable_feature = true,
          .set_random_coin_flip_for_navigation = false,
          .set_coin_flip_override = true,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kNotSet,
          .initial_state = content::CLIENT_LOFI_ON,
          .want_returned = content::CLIENT_LOFI_ON,
      },
      {
          .msg = "After-commit decided previews are not affected before commit "
                 "on false coin flip",
          .enable_feature = true,
          .set_random_coin_flip_for_navigation = false,
          .set_coin_flip_override = false,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kNotSet,
          .initial_state = content::CLIENT_LOFI_ON,
          .want_returned = content::CLIENT_LOFI_ON,
      },
      {
          .msg =
              "Before-commit decided previews are affected on true coin flip",
          .enable_feature = true,
          .set_random_coin_flip_for_navigation = true,
          .set_coin_flip_override = false,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kHoldback,
          .initial_state = content::OFFLINE_PAGE_ON,
          .want_returned = content::PREVIEWS_OFF,
      },
      {
          .msg =
              "Before-commit decided previews are affected on forced override",
          .enable_feature = true,
          .set_random_coin_flip_for_navigation = false,
          .set_coin_flip_override = true,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kHoldback,
          .initial_state = content::OFFLINE_PAGE_ON,
          .want_returned = content::PREVIEWS_OFF,
      },
      {
          .msg = "Before-commit decided previews are logged on false coin flip",
          .enable_feature = true,
          .set_random_coin_flip_for_navigation = false,
          .set_coin_flip_override = false,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kAllowed,
          .initial_state = content::OFFLINE_PAGE_ON,
          .want_returned = content::OFFLINE_PAGE_ON,
      },
      {
          .msg =
              "True coin flip impacts both pre and post commit previews when "
              "both exist",
          .enable_feature = true,
          .set_random_coin_flip_for_navigation = true,
          .set_coin_flip_override = false,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kHoldback,
          .initial_state = content::OFFLINE_PAGE_ON | content::CLIENT_LOFI_ON,
          .want_returned = content::PREVIEWS_OFF,
      },
      {
          .msg =
              "Forced override impacts both pre and post commit previews when "
              "both exist",
          .enable_feature = true,
          .set_random_coin_flip_for_navigation = false,
          .set_coin_flip_override = true,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kHoldback,
          .initial_state = content::OFFLINE_PAGE_ON | content::CLIENT_LOFI_ON,
          .want_returned = content::PREVIEWS_OFF,
      },
      {
          .msg = "False coin flip logs both pre and post commit previews when "
                 "both exist",
          .enable_feature = true,
          .set_random_coin_flip_for_navigation = false,
          .set_coin_flip_override = false,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kAllowed,
          .initial_state = content::OFFLINE_PAGE_ON | content::CLIENT_LOFI_ON,
          .want_returned = content::OFFLINE_PAGE_ON | content::CLIENT_LOFI_ON,
      },
  };

  for (const TestCase& test_case : kTestCases) {
    SCOPED_TRACE(test_case.msg);

    // Starting the navigation will cause content to call into
    // |MaybeCoinFlipHoldbackBeforeCommit| as part of the navigation simulation.
    // So don't enable the feature until afterwards.
    content::NavigationHandle* handle = StartNavigation();

    GetPreviewsUserData(handle)->SetRandomCoinFlipForNavigationForTesting(
        test_case.set_random_coin_flip_for_navigation);

    base::test::ScopedFeatureList scoped_feature_list;
    if (test_case.enable_feature) {
      scoped_feature_list.InitAndEnableFeatureWithParameters(
          previews::features::kCoinFlipHoldback,
          {{"force_coin_flip_always_holdback",
            test_case.set_coin_flip_override ? "true" : "false"}});
    } else {
      scoped_feature_list.InitAndDisableFeature(
          previews::features::kCoinFlipHoldback);
    }

    content::PreviewsState returned =
        MaybeCoinFlipHoldbackBeforeCommit(test_case.initial_state, handle);

    EXPECT_EQ(test_case.want_returned, returned);
    EXPECT_EQ(test_case.want_coin_flip_result,
              GetPreviewsUserData(handle)->coin_flip_holdback_result());
  }
}

TEST_F(PreviewsContentSimulatedNavigationTest, TestCoinFlipAfterCommit) {
  struct TestCase {
    std::string msg;
    bool enable_feature;
    bool set_random_coin_flip_for_navigation;
    bool set_coin_flip_override;
    previews::CoinFlipHoldbackResult want_coin_flip_result;
    content::PreviewsState initial_state;
    content::PreviewsState want_returned;
  };
  const TestCase kTestCases[]{
      {
          .msg = "Feature disabled, no affect, heads",
          .enable_feature = false,
          .set_random_coin_flip_for_navigation = true,
          .set_coin_flip_override = false,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kNotSet,
          .initial_state = content::CLIENT_LOFI_ON,
          .want_returned = content::CLIENT_LOFI_ON,
      },
      {
          .msg = "Feature disabled, no affect, tails",
          .enable_feature = false,
          .set_random_coin_flip_for_navigation = false,
          .set_coin_flip_override = false,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kNotSet,
          .initial_state = content::CLIENT_LOFI_ON,
          .want_returned = content::CLIENT_LOFI_ON,
      },
      {
          .msg = "Feature disabled, no affect, forced override",
          .enable_feature = false,
          .set_random_coin_flip_for_navigation = false,
          .set_coin_flip_override = true,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kNotSet,
          .initial_state = content::CLIENT_LOFI_ON,
          .want_returned = content::CLIENT_LOFI_ON,
      },
      {
          .msg = "Holdback enabled previews",
          .enable_feature = true,
          .set_random_coin_flip_for_navigation = true,
          .set_coin_flip_override = false,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kHoldback,
          .initial_state = content::CLIENT_LOFI_ON,
          .want_returned = content::PREVIEWS_OFF,
      },
      {
          .msg = "Holdback enabled previews via override",
          .enable_feature = true,
          .set_random_coin_flip_for_navigation = false,
          .set_coin_flip_override = true,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kHoldback,
          .initial_state = content::CLIENT_LOFI_ON,
          .want_returned = content::PREVIEWS_OFF,
      },
      {
          .msg = "Log enabled previews",
          .enable_feature = true,
          .set_random_coin_flip_for_navigation = false,
          .set_coin_flip_override = false,
          .want_coin_flip_result = previews::CoinFlipHoldbackResult::kAllowed,
          .initial_state = content::CLIENT_LOFI_ON,
          .want_returned = content::CLIENT_LOFI_ON,
      },
  };

  for (const TestCase& test_case : kTestCases) {
    SCOPED_TRACE(test_case.msg);

    // Starting the navigation will cause content to call into
    // |MaybeCoinFlipHoldbackBeforeCommit| as part of the navigation simulation.
    // So don't enable the feature until afterwards.
    content::NavigationHandle* handle = StartNavigationAndReadyCommit();

    GetPreviewsUserData(handle)->SetRandomCoinFlipForNavigationForTesting(
        test_case.set_random_coin_flip_for_navigation);

    base::test::ScopedFeatureList scoped_feature_list;
    if (test_case.enable_feature) {
      scoped_feature_list.InitAndEnableFeatureWithParameters(
          previews::features::kCoinFlipHoldback,
          {{"force_coin_flip_always_holdback",
            test_case.set_coin_flip_override ? "true" : "false"}});
    } else {
      scoped_feature_list.InitAndDisableFeature(
          previews::features::kCoinFlipHoldback);
    }

    content::PreviewsState returned =
        MaybeCoinFlipHoldbackAfterCommit(test_case.initial_state, handle);

    EXPECT_EQ(test_case.want_returned, returned);
    EXPECT_EQ(test_case.want_coin_flip_result,
              GetPreviewsUserData(handle)->coin_flip_holdback_result());
  }
}

}  // namespace

}  // namespace previews
