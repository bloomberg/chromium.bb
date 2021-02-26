// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/login_detection/login_detection_tab_helper.h"
#include "chrome/browser/login_detection/login_detection_type.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/site_isolation/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace login_detection {

class LoginDetectionBrowserTest : public InProcessBrowserTest {
 public:
  LoginDetectionBrowserTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kLoginDetection, {}},
         {site_isolation::features::kSiteIsolationForPasswordSites, {}}},
        {});
  }

  void SetUpOnMainThread() override {
    https_test_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_test_server_.Start());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void ResetHistogramTester() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  // Verifies the histograms for the given login detection type to be recorded.
  void ExpectLoginDetectionTypeMetric(LoginDetectionType type) {
    histogram_tester_->ExpectUniqueSample("Login.PageLoad.DetectionType", type,
                                          1);
  }

 protected:
  net::EmbeddedTestServer https_test_server_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LoginDetectionBrowserTest, PopUpBasedOAuthLoginFlow) {
  // Navigate to the OAuth requestor.
  ui_test_utils::NavigateToURL(
      browser(), https_test_server_.GetURL("www.foo.com", "/title1.html"));
  ExpectLoginDetectionTypeMetric(
      LoginDetectionTabHelper::LoginDetectionType::kNoLogin);
  ResetHistogramTester();

  // Create a popup for the navigation flow.
  content::WebContentsAddedObserver web_contents_added_observer;
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::JsReplace(
          "window.open($1, 'oauth_window', 'width=10,height=10');",
          https_test_server_.GetURL("www.oauthprovider.com",
                                    "/title2.html?client_id=123"))));
  auto* popup_contents = web_contents_added_observer.GetWebContents();
  content::TestNavigationObserver observer(popup_contents);
  observer.WaitForNavigationFinished();
  // This popup navigation is treated as not logged-in too.
  ExpectLoginDetectionTypeMetric(
      LoginDetectionTabHelper::LoginDetectionType::kNoLogin);
  ResetHistogramTester();

  // When the popup is closed, it will be detected as OAuth login.
  content::WebContentsDestroyedWatcher destroyed_watcher(popup_contents);
  EXPECT_TRUE(ExecJs(popup_contents, "window.close()"));
  destroyed_watcher.Wait();
  ExpectLoginDetectionTypeMetric(
      LoginDetectionTabHelper::LoginDetectionType::kOauthPopUpFirstTimeLoginFlow);
  ResetHistogramTester();

  // Subsequent navigations to the OAuth requestor site will be treated as OAuth
  ui_test_utils::NavigateToURL(
      browser(), https_test_server_.GetURL("www.foo.com", "/title3.html"));
  ExpectLoginDetectionTypeMetric(
      LoginDetectionTabHelper::LoginDetectionType::kOauthLogin);
}

}  // namespace login_detection
