// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_client_side_detection_host_delegate.h"

#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_safe_browsing_blocking_page_factory.h"
#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace {

using ::testing::_;
using ::testing::StrictMock;

class FakeClientSideDetectionService : public ClientSideDetectionService {
 public:
  FakeClientSideDetectionService() : ClientSideDetectionService(nullptr) {}

  void SendClientReportPhishingRequest(
      std::unique_ptr<ClientPhishingRequest> verdict,
      ClientReportPhishingRequestCallback callback,
      const std::string& access_token) override {
    saved_request_ = *verdict;
    saved_callback_ = std::move(callback);
    access_token_ = access_token;
    request_callback_.Run();
  }

  const ClientPhishingRequest& saved_request() { return saved_request_; }

  bool saved_callback_is_null() { return saved_callback_.is_null(); }

  ClientReportPhishingRequestCallback saved_callback() {
    return std::move(saved_callback_);
  }

  void SetModel(const ClientSideModel& model) { model_ = model; }

  const std::string& GetModelStr() override {
    client_side_model_ = model_.SerializeAsString();
    return client_side_model_;
  }

  void SetRequestCallback(const base::RepeatingClosure& closure) {
    request_callback_ = closure;
  }

 private:
  ClientPhishingRequest saved_request_;
  ClientReportPhishingRequestCallback saved_callback_;
  ClientSideModel model_;
  std::string access_token_;
  std::string client_side_model_;
  base::RepeatingClosure request_callback_;
};

class MockSafeBrowsingUIManager : public SafeBrowsingUIManager {
 public:
  MockSafeBrowsingUIManager()
      : SafeBrowsingUIManager(
            std::make_unique<ChromeSafeBrowsingUIManagerDelegate>(),
            std::make_unique<ChromeSafeBrowsingBlockingPageFactory>(),
            GURL(chrome::kChromeUINewTabURL)) {}

  MockSafeBrowsingUIManager(const MockSafeBrowsingUIManager&) = delete;
  MockSafeBrowsingUIManager& operator=(const MockSafeBrowsingUIManager&) =
      delete;

  MOCK_METHOD1(DisplayBlockingPage, void(const UnsafeResource& resource));

 protected:
  ~MockSafeBrowsingUIManager() override = default;
};

}  // namespace

class ClientSideDetectionHostBrowserTest : public InProcessBrowserTest {
 public:
  ClientSideDetectionHostBrowserTest() = default;
  ~ClientSideDetectionHostBrowserTest() override = default;

  void SetUpOnMainThread() override {
    model_.set_version(123);
    model_.set_max_words_per_term(1);
    VisualTarget* target = model_.mutable_vision_model()->add_targets();
    target->set_digest("target1_digest");
    // Create a hash corresponding to a blank screen.
    std::string hash = "\x30";
    for (int i = 0; i < 288; i++)
      hash += "\xff";
    target->set_hash(hash);
    target->set_dimension_size(48);
    MatchRule* match_rule = target->mutable_match_config()->add_match_rule();
    // The actual hash distance is 76, so set the distance to 200 for safety. A
    // completely random bitstring would expect a Hamming distance of 1152.
    match_rule->set_hash_distance(200);
  }

  ClientSideModel& client_side_model() { return model_; }

 private:
  ClientSideModel model_;
};

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostBrowserTest,
                       VerifyVisualFeatureCollection) {
  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  ASSERT_TRUE(embedded_test_server()->Start());
  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(&fake_csd_service);
  csd_host->SendModelToRenderFrame();
  csd_host->set_ui_manager(mock_ui_manager.get());

  GURL page_url(embedded_test_server()->GetURL("/safe_browsing/malware.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  base::RunLoop run_loop;
  fake_csd_service.SetRequestCallback(run_loop.QuitClosure());

  // Bypass the pre-classification checks
  csd_host->OnPhishingPreClassificationDone(/*should_classify=*/true);

  run_loop.Run();

  ASSERT_FALSE(fake_csd_service.saved_callback_is_null());

  EXPECT_EQ(fake_csd_service.saved_request().model_version(), 123);
  ASSERT_EQ(fake_csd_service.saved_request().vision_match_size(), 1);
  EXPECT_EQ(
      fake_csd_service.saved_request().vision_match(0).matched_target_digest(),
      "target1_digest");

  // Expect an interstitial to be shown
  EXPECT_CALL(*mock_ui_manager, DisplayBlockingPage(_));
  std::move(fake_csd_service.saved_callback()).Run(page_url, true);
}

class ClientSideDetectionHostPrerenderBrowserTest
    : public ClientSideDetectionHostBrowserTest {
 public:
  ClientSideDetectionHostPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &ClientSideDetectionHostPrerenderBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~ClientSideDetectionHostPrerenderBrowserTest() override = default;
  ClientSideDetectionHostPrerenderBrowserTest(
      const ClientSideDetectionHostPrerenderBrowserTest&) = delete;
  ClientSideDetectionHostPrerenderBrowserTest& operator=(
      const ClientSideDetectionHostPrerenderBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    ClientSideDetectionHostBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ClientSideDetectionHostBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostPrerenderBrowserTest,
                       PrerenderShouldNotAffectClientSideDetection) {
  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(&fake_csd_service);
  csd_host->SendModelToRenderFrame();
  csd_host->set_ui_manager(mock_ui_manager.get());

  GURL page_url(embedded_test_server()->GetURL("/safe_browsing/malware.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  base::RunLoop run_loop;
  fake_csd_service.SetRequestCallback(run_loop.QuitClosure());

  // Bypass the pre-classification checks.
  csd_host->OnPhishingPreClassificationDone(/*should_classify=*/true);

  // A prerendered navigation committing should not cancel classification.
  // We simulate the commit of a prerendered navigation to avoid races
  // between the completion of phishing detection in the primary
  // main frame's renderer and the commit of a real prerendered navigation.
  // TODO(mcnee): Use a real prerendered navigation here and make sure the
  // navigation doesn't race with the classification.
  content::MockNavigationHandle prerendered_navigation_handle;
  prerendered_navigation_handle.set_has_committed(true);
  prerendered_navigation_handle.set_is_in_primary_main_frame(false);
  csd_host->DidFinishNavigation(&prerendered_navigation_handle);

  run_loop.Run();

  ASSERT_FALSE(fake_csd_service.saved_callback_is_null());

  EXPECT_EQ(fake_csd_service.saved_request().model_version(), 123);
  ASSERT_EQ(fake_csd_service.saved_request().vision_match_size(), 1);
  EXPECT_EQ(
      fake_csd_service.saved_request().vision_match(0).matched_target_digest(),
      "target1_digest");

  // Expect an interstitial to be shown.
  EXPECT_CALL(*mock_ui_manager, DisplayBlockingPage(_));
  std::move(fake_csd_service.saved_callback()).Run(page_url, true);
}

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostPrerenderBrowserTest,
                       ClassifyPrerenderedPageAfterActivation) {
  FakeClientSideDetectionService fake_csd_service;
  fake_csd_service.SetModel(client_side_model());

  scoped_refptr<StrictMock<MockSafeBrowsingUIManager>> mock_ui_manager =
      new StrictMock<MockSafeBrowsingUIManager>();

  std::unique_ptr<ClientSideDetectionHost> csd_host =
      ChromeClientSideDetectionHostDelegate::CreateHost(
          browser()->tab_strip_model()->GetActiveWebContents());
  csd_host->set_client_side_detection_service(&fake_csd_service);
  csd_host->SendModelToRenderFrame();
  csd_host->set_ui_manager(mock_ui_manager.get());

  base::RunLoop run_loop;
  fake_csd_service.SetRequestCallback(run_loop.QuitClosure());

  const GURL initial_url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Prerender then activate a phishing page.
  const GURL prerender_url =
      embedded_test_server()->GetURL("/safe_browsing/malware.html");
  prerender_helper().AddPrerender(prerender_url);
  prerender_helper().NavigatePrimaryPage(prerender_url);

  // Bypass the pre-classification checks.
  csd_host->OnPhishingPreClassificationDone(/*should_classify=*/true);

  run_loop.Run();

  ASSERT_FALSE(fake_csd_service.saved_callback_is_null());

  EXPECT_EQ(fake_csd_service.saved_request().model_version(), 123);
  ASSERT_EQ(fake_csd_service.saved_request().vision_match_size(), 1);
  EXPECT_EQ(
      fake_csd_service.saved_request().vision_match(0).matched_target_digest(),
      "target1_digest");

  // Expect an interstitial to be shown.
  EXPECT_CALL(*mock_ui_manager, DisplayBlockingPage(_));
  std::move(fake_csd_service.saved_callback()).Run(prerender_url, true);
}

}  // namespace safe_browsing
