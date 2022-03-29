// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {

namespace {
constexpr char kBaseDataDir[] = "content/test/data/attribution_reporting/";
}

class PrivacySandboxAdsAPIsBrowserTestBase : public ContentBrowserTest {
 public:
  PrivacySandboxAdsAPIsBrowserTestBase() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // the origin trial token in the response is associated with a fixed
    // origin, whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
            [&](URLLoaderInterceptor::RequestParams* params) -> bool {
              URLLoaderInterceptor::WriteResponse(
                  base::StrCat(
                      {kBaseDataDir, params->url_request.url.path_piece()}),
                  params->client.get());
              return true;
            }));
  }

  void TearDownOnMainThread() override { url_loader_interceptor_.reset(); }

  WebContents* web_contents() { return shell()->web_contents(); }

 private:
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
};

class PrivacySandboxAdsAPIsAllEnabledBrowserTest
    : public PrivacySandboxAdsAPIsBrowserTestBase {
 public:
  PrivacySandboxAdsAPIsAllEnabledBrowserTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kPrivacySandboxAdsAPIs,
         blink::features::kBrowsingTopics, blink::features::kFledge},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsAllEnabledBrowserTest,
                       OriginTrialEnabled_FeatureDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "attribution-reporting')"));
  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "browsing-topics')"));
  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "join-ad-interest-group')"));

  EXPECT_EQ(true, EvalJs(shell(), "window.attributionReporting !== undefined"));
  EXPECT_EQ(true, EvalJs(shell(), "document.browsingTopics !== undefined"));
  EXPECT_EQ(true, EvalJs(shell(), "navigator.runAdAuction !== undefined"));
  EXPECT_EQ(true,
            EvalJs(shell(), "navigator.joinAdInterestGroup !== undefined"));
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsAllEnabledBrowserTest,
                       OriginTrialDisabled_FeatureNotDetected) {
  // Navigate to a page without an OT token.
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_without_ads_apis_ot.html")));

  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "attribution-reporting')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "browsing-topics')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "join-ad-interest-group')"));

  EXPECT_EQ(true, EvalJs(shell(), "window.attributionReporting === undefined"));
  EXPECT_EQ(true, EvalJs(shell(), "document.browsingTopics === undefined"));
  EXPECT_EQ(true, EvalJs(shell(), "navigator.runAdAuction === undefined"));
  EXPECT_EQ(true,
            EvalJs(shell(), "navigator.joinAdInterestGroup === undefined"));
}

class PrivacySandboxAdsAPIsTopicsDisabledBrowserTest
    : public PrivacySandboxAdsAPIsBrowserTestBase {
 public:
  PrivacySandboxAdsAPIsTopicsDisabledBrowserTest() {
    feature_list_.InitWithFeatures({blink::features::kPrivacySandboxAdsAPIs},
                                   {blink::features::kBrowsingTopics});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsTopicsDisabledBrowserTest,
                       OriginTrialEnabled_CorrectFeaturesDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "attribution-reporting')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "browsing-topics')"));

  EXPECT_EQ(true, EvalJs(shell(), "window.attributionReporting !== undefined"));
  EXPECT_EQ(false, EvalJs(shell(), "document.browsingTopics !== undefined"));
}

class PrivacySandboxAdsAPIsFledgeDisabledBrowserTest
    : public PrivacySandboxAdsAPIsBrowserTestBase {
 public:
  PrivacySandboxAdsAPIsFledgeDisabledBrowserTest() {
    feature_list_.InitWithFeatures({blink::features::kPrivacySandboxAdsAPIs},
                                   {blink::features::kFledge});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsFledgeDisabledBrowserTest,
                       OriginTrialEnabled_CorrectFeaturesDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_EQ(true, EvalJs(shell(),
                         "document.featurePolicy.features().includes('"
                         "attribution-reporting')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "join-ad-interest-group')"));

  EXPECT_EQ(true, EvalJs(shell(), "window.attributionReporting !== undefined"));
  EXPECT_EQ(false, EvalJs(shell(), "navigator.runAdAuction !== undefined"));
  EXPECT_EQ(false,
            EvalJs(shell(), "navigator.joinAdInterestGroup !== undefined"));
}

class PrivacySandboxAdsAPIsDisabledBrowserTest
    : public PrivacySandboxAdsAPIsBrowserTestBase {
 public:
  PrivacySandboxAdsAPIsDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kPrivacySandboxAdsAPIs);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdsAPIsDisabledBrowserTest,
                       BaseFeatureDisabled_FeatureNotDetected) {
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://example.test/page_with_ads_apis_ot.html")));

  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "attribution-reporting')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "browsing-topics')"));
  EXPECT_EQ(false, EvalJs(shell(),
                          "document.featurePolicy.features().includes('"
                          "join-ad-interest-group')"));

  EXPECT_EQ(true, EvalJs(shell(), "window.attributionReporting === undefined"));
  EXPECT_EQ(true, EvalJs(shell(), "document.browsingTopics === undefined"));
  EXPECT_EQ(true, EvalJs(shell(), "navigator.runAdAuction === undefined"));
  EXPECT_EQ(true,
            EvalJs(shell(), "navigator.joinAdInterestGroup === undefined"));
}

}  // namespace content
