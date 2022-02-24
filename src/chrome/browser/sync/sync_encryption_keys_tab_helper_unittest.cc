// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_encryption_keys_tab_helper.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/sync_encryption_keys_extension.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace {

using testing::IsNull;
using testing::NotNull;

class SyncEncryptionKeysTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  SyncEncryptionKeysTabHelperTest(const SyncEncryptionKeysTabHelperTest&) =
      delete;
  SyncEncryptionKeysTabHelperTest& operator=(
      const SyncEncryptionKeysTabHelperTest&) = delete;

 protected:
  SyncEncryptionKeysTabHelperTest() = default;

  ~SyncEncryptionKeysTabHelperTest() override = default;

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SyncEncryptionKeysTabHelper::CreateForWebContents(web_contents());
  }

  bool HasEncryptionKeysApi(content::RenderFrameHost* rfh) {
    auto* tab_helper =
        SyncEncryptionKeysTabHelper::FromWebContents(web_contents());
    return tab_helper->HasEncryptionKeysApiForTesting(rfh);
  }

  bool HasEncryptionKeysApiInMainFrame() {
    return HasEncryptionKeysApi(main_rfh());
  }

  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {{SyncServiceFactory::GetInstance(),
             SyncServiceFactory::GetDefaultFactory()},
            {ChromeSigninClientFactory::GetInstance(),
             base::BindRepeating(&signin::BuildTestSigninClient)}};
  }
};

TEST_F(SyncEncryptionKeysTabHelperTest, ShouldExposeMojoApiToAllowedOrigin) {
  ASSERT_FALSE(HasEncryptionKeysApiInMainFrame());
  web_contents_tester()->NavigateAndCommit(GaiaUrls::GetInstance()->gaia_url());
  EXPECT_TRUE(HasEncryptionKeysApiInMainFrame());
}

TEST_F(SyncEncryptionKeysTabHelperTest,
       ShouldNotExposeMojoApiToUnallowedOrigin) {
  web_contents_tester()->NavigateAndCommit(GURL("http://page.com"));
  EXPECT_FALSE(HasEncryptionKeysApiInMainFrame());
}

TEST_F(SyncEncryptionKeysTabHelperTest, ShouldNotExposeMojoApiIfNavigatedAway) {
  web_contents_tester()->NavigateAndCommit(GaiaUrls::GetInstance()->gaia_url());
  ASSERT_TRUE(HasEncryptionKeysApiInMainFrame());
  web_contents_tester()->NavigateAndCommit(GURL("http://page.com"));
  EXPECT_FALSE(HasEncryptionKeysApiInMainFrame());
}

TEST_F(SyncEncryptionKeysTabHelperTest,
       ShouldExposeMojoApiEvenIfSubframeNavigatedAway) {
  web_contents_tester()->NavigateAndCommit(GaiaUrls::GetInstance()->gaia_url());
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  ASSERT_TRUE(HasEncryptionKeysApiInMainFrame());

  content::NavigationSimulator::CreateRendererInitiated(GURL("http://page.com"),
                                                        subframe)
      ->Commit();
  // For the receiver set to be fully updated, a mainframe navigation is needed.
  // Otherwise the test passes regardless of whether the logic is buggy.
  web_contents_tester()->NavigateAndCommit(GaiaUrls::GetInstance()->gaia_url());
  EXPECT_TRUE(HasEncryptionKeysApiInMainFrame());
}

TEST_F(SyncEncryptionKeysTabHelperTest,
       ShouldNotExposeMojoApiIfNavigationFailed) {
  auto* render_frame_host =
      content::NavigationSimulator::NavigateAndFailFromBrowser(
          web_contents(), GaiaUrls::GetInstance()->gaia_url(),
          net::ERR_ABORTED);
  EXPECT_FALSE(HasEncryptionKeysApi(render_frame_host));
  EXPECT_FALSE(HasEncryptionKeysApiInMainFrame());
}

TEST_F(SyncEncryptionKeysTabHelperTest,
       ShouldNotExposeMojoApiIfNavigatedAwayToErrorPage) {
  web_contents_tester()->NavigateAndCommit(GaiaUrls::GetInstance()->gaia_url());
  ASSERT_TRUE(HasEncryptionKeysApiInMainFrame());

  auto* render_frame_host =
      content::NavigationSimulator::NavigateAndFailFromBrowser(
          web_contents(), GURL("http://page.com"), net::ERR_ABORTED);
  EXPECT_FALSE(HasEncryptionKeysApi(render_frame_host));
  // `net::ERR_ABORTED` doesn't update the main frame and the previous main
  // frame is still available. So, EncryptionKeysApi is still valid on the main
  // frame.
  EXPECT_TRUE(HasEncryptionKeysApiInMainFrame());

  render_frame_host = content::NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), GURL("http://page.com"), net::ERR_TIMED_OUT);
  EXPECT_FALSE(HasEncryptionKeysApi(render_frame_host));
  // `net::ERR_TIMED_OUT` commits the error page that is the main frame now.
  EXPECT_FALSE(HasEncryptionKeysApiInMainFrame());
}

class SyncEncryptionKeysTabHelperPrerenderingTest
    : public SyncEncryptionKeysTabHelperTest {
 public:
  SyncEncryptionKeysTabHelperPrerenderingTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kPrerender2},
        // Disable the memory requirement of Prerender2 so the test can run on
        // any bot.
        {blink::features::kPrerender2MemoryControls});
  }
  ~SyncEncryptionKeysTabHelperPrerenderingTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that EncryptionKeys works based on a main frame. A prerendered page
// also creates EncryptionKeys as it's a main frame. But, if EncryptionKeys
// is accessed thrugh Mojo in prerendering, it causes canceling prerendering.
// See the browser test, 'ShouldNotBindEncryptionKeysApiInPrerendering', from
// 'sync_encryption_keys_tab_helper_browsertest.cc' for the details about
// canceling prerendering.
TEST_F(SyncEncryptionKeysTabHelperPrerenderingTest,
       CreateEncryptionKeysInPrerendering) {
  // Load a page.
  ASSERT_FALSE(HasEncryptionKeysApiInMainFrame());
  web_contents_tester()->NavigateAndCommit(GaiaUrls::GetInstance()->gaia_url());
  ASSERT_TRUE(HasEncryptionKeysApiInMainFrame());

  // If prerendering happens in the gaia url, EncryptionKeys is created for
  // the prerendering and the current EncryptionKeys for a primary page also
  // exists.
  const GURL kPrerenderingUrl(GaiaUrls::GetInstance()->gaia_url().spec() +
                              "?prerendering");
  auto* prerender_rfh = content::WebContentsTester::For(web_contents())
                            ->AddPrerenderAndCommitNavigation(kPrerenderingUrl);
  DCHECK_NE(prerender_rfh, nullptr);
  EXPECT_TRUE(HasEncryptionKeysApi(prerender_rfh));
  EXPECT_TRUE(HasEncryptionKeysApiInMainFrame());

  content::test::PrerenderHostObserver host_observer(*web_contents(),
                                                     kPrerenderingUrl);

  // Activate the prerendered page.
  auto* activated_rfh =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          kPrerenderingUrl, web_contents()->GetMainFrame());
  host_observer.WaitForActivation();
  EXPECT_TRUE(host_observer.was_activated());
  // The EncryptionKeys exists for the activated page.
  EXPECT_TRUE(HasEncryptionKeysApi(activated_rfh));

  // If the prerendering happens to the cross origin, the prerendering would be
  // canceled.
  const GURL kCrossOriginPrerenderingUrl(GURL("http://page.com"));
  int frame_tree_node_id = content::WebContentsTester::For(web_contents())
                               ->AddPrerender(kCrossOriginPrerenderingUrl);
  ASSERT_EQ(frame_tree_node_id, content::RenderFrameHost::kNoFrameTreeNodeId);
  // EncryptionKeys is still valid in a primary page.
  EXPECT_TRUE(HasEncryptionKeysApiInMainFrame());

  // Load a page that doesn't allow EncryptionKeys.
  auto* rfh = content::NavigationSimulator::NavigateAndCommitFromDocument(
      kCrossOriginPrerenderingUrl, web_contents()->GetMainFrame());
  EXPECT_FALSE(HasEncryptionKeysApi(rfh));
  EXPECT_FALSE(HasEncryptionKeysApiInMainFrame());
}
}  // namespace
