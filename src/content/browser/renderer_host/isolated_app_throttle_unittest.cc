// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/isolated_app_throttle.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/page_type.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace content {

namespace {

const char kAppUrl[] = "https://isolated.app";
const char kAppUrl2[] = "https://isolated.app/page";
const char kNonAppUrl[] = "https://example.com";
const char kNonAppUrl2[] = "https://example.com/page";
static constexpr RenderFrameHost::WebExposedIsolationLevel kNotIsolated =
    RenderFrameHost::WebExposedIsolationLevel::kNotIsolated;
static constexpr RenderFrameHost::WebExposedIsolationLevel
    kMaybeIsolatedApplication =
        RenderFrameHost::WebExposedIsolationLevel::kMaybeIsolatedApplication;

class IsolatedAppContentBrowserClient : public ContentBrowserClient {
 public:
  bool ShouldUrlUseApplicationIsolationLevel(BrowserContext* browser_context,
                                             const GURL& url) override {
    return url::Origin::Create(url) == url::Origin::Create(GURL(kAppUrl));
  }
};

}  // namespace

class IsolatedAppThrottleTest : public RenderViewHostTestHarness {
 public:
  IsolatedAppThrottleTest()
      : feature_list_(blink::features::kWebAppEnableIsolatedStorage) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    old_client_ = SetBrowserClientForTesting(&test_client_);

    // COOP/COEP headers must be sent to enable application isolation.
    coop_coep_headers_ =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    coop_coep_headers_->AddHeader("Cross-Origin-Embedder-Policy",
                                  "require-corp");
    coop_coep_headers_->AddHeader("Cross-Origin-Opener-Policy", "same-origin");

    // CORP/COEP headers must be sent to enable embedding.
    corp_coep_headers_ =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    corp_coep_headers_->AddHeader("Cross-Origin-Embedder-Policy",
                                  "require-corp");
    corp_coep_headers_->AddHeader("Cross-Origin-Resource-Policy",
                                  "cross-origin");
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);

    RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<NavigationSimulator> StartBrowserInitiatedNavigation(
      const char* url) {
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(GURL(url), web_contents());
    simulator->Start();
    return simulator;
  }

  void CommitBrowserInitiatedNavigation(
      const char* url,
      scoped_refptr<net::HttpResponseHeaders> response_headers = nullptr) {
    CommitNavigation(StartBrowserInitiatedNavigation(url), main_frame_id(), url,
                     response_headers);
  }

  std::unique_ptr<NavigationSimulator> StartRendererInitiatedNavigation(
      int frame_tree_node_id,
      const char* url) {
    RenderFrameHost* rfh = FrameTreeNode::GloballyFindByID(frame_tree_node_id)
                               ->current_frame_host();
    auto simulator =
        NavigationSimulator::CreateRendererInitiated(GURL(url), rfh);
    simulator->Start();
    return simulator;
  }

  void CommitRendererInitiatedNavigation(
      int frame_tree_node_id,
      const char* url,
      scoped_refptr<net::HttpResponseHeaders> response_headers = nullptr) {
    CommitNavigation(StartRendererInitiatedNavigation(frame_tree_node_id, url),
                     frame_tree_node_id, url, response_headers);
  }

  int CreateIframe(int parent_frame_tree_node_id, const std::string& name) {
    TestRenderFrameHost* parent_rfh = static_cast<TestRenderFrameHost*>(
        FrameTreeNode::GloballyFindByID(parent_frame_tree_node_id)
            ->current_frame_host());
    RenderFrameHost* child_rfh = parent_rfh->AppendChild(name);
    return child_rfh->GetFrameTreeNodeId();
  }

  RenderFrameHost::WebExposedIsolationLevel GetWebExposedIsolationLevel(
      int frame_tree_node_id) {
    return FrameTreeNode::GloballyFindByID(frame_tree_node_id)
        ->current_frame_host()
        ->GetWebExposedIsolationLevel();
  }

  int main_frame_id() { return main_rfh()->GetFrameTreeNodeId(); }

  scoped_refptr<net::HttpResponseHeaders> coop_coep_headers() {
    return coop_coep_headers_;
  }

  scoped_refptr<net::HttpResponseHeaders> corp_coep_headers() {
    return corp_coep_headers_;
  }

  static PageType GetPageType(RenderFrameHost* render_frame_host) {
    return FrameTreeNode::From(render_frame_host)
        ->navigator()
        .controller()
        .GetLastCommittedEntry()
        ->GetPageType();
  }

 private:
  void CommitNavigation(
      std::unique_ptr<NavigationSimulator> simulator,
      int frame_tree_node_id,
      const char* url,
      scoped_refptr<net::HttpResponseHeaders> response_headers) {
    // Verify that the Start call was successful.
    auto start_result = simulator->GetLastThrottleCheckResult();
    CHECK_EQ(NavigationThrottle::PROCEED, start_result.action());

    if (response_headers)
      simulator->SetResponseHeaders(response_headers);
    simulator->Commit();

    RenderFrameHost* rfh = FrameTreeNode::GloballyFindByID(frame_tree_node_id)
                               ->current_frame_host();
    auto commit_result = simulator->GetLastThrottleCheckResult();
    CHECK_EQ(NavigationThrottle::PROCEED, commit_result.action());
    CHECK_EQ(GURL(url), rfh->GetLastCommittedURL());
  }

  base::test::ScopedFeatureList feature_list_;
  IsolatedAppContentBrowserClient test_client_;
  raw_ptr<ContentBrowserClient> old_client_;
  scoped_refptr<net::HttpResponseHeaders> coop_coep_headers_;
  scoped_refptr<net::HttpResponseHeaders> corp_coep_headers_;
};

TEST_F(IsolatedAppThrottleTest, AllowNavigationWithinNonApp) {
  CommitBrowserInitiatedNavigation(kNonAppUrl);
  EXPECT_EQ(kNotIsolated, GetWebExposedIsolationLevel(main_frame_id()));

  CommitRendererInitiatedNavigation(main_frame_id(), kNonAppUrl2);
  EXPECT_EQ(kNotIsolated, GetWebExposedIsolationLevel(main_frame_id()));
}

TEST_F(IsolatedAppThrottleTest, BlockNavigationIntoIsolatedApp) {
  CommitBrowserInitiatedNavigation(kNonAppUrl);
  EXPECT_EQ(kNotIsolated, GetWebExposedIsolationLevel(main_frame_id()));

  auto simulator = StartRendererInitiatedNavigation(main_frame_id(), kAppUrl);

  auto start_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::BLOCK_REQUEST, start_result.action());
}

TEST_F(IsolatedAppThrottleTest, AllowNavigationIfNoPreviousPage) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kMaybeIsolatedApplication,
            GetWebExposedIsolationLevel(main_frame_id()));
}

TEST_F(IsolatedAppThrottleTest, AllowNavigationWithinIsolatedApp) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kMaybeIsolatedApplication,
            GetWebExposedIsolationLevel(main_frame_id()));

  CommitRendererInitiatedNavigation(main_frame_id(), kAppUrl2,
                                    coop_coep_headers());
  EXPECT_EQ(kMaybeIsolatedApplication,
            GetWebExposedIsolationLevel(main_frame_id()));
}

TEST_F(IsolatedAppThrottleTest, CancelCrossOriginNavigation) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kMaybeIsolatedApplication,
            GetWebExposedIsolationLevel(main_frame_id()));

  auto simulator =
      StartRendererInitiatedNavigation(main_frame_id(), kNonAppUrl);

  auto start_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::CANCEL, start_result.action());
}

TEST_F(IsolatedAppThrottleTest, BlockNavigationWithinIsolatedAppWithBadCoop) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kMaybeIsolatedApplication,
            GetWebExposedIsolationLevel(main_frame_id()));

  auto simulator = StartRendererInitiatedNavigation(main_frame_id(), kAppUrl2);

  auto start_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::PROCEED, start_result.action());

  // Don't set COEP/COOP headers, which will prevent the navigation from having
  // application isolation.
  simulator->ReadyToCommit();

  auto response_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::BLOCK_RESPONSE, response_result.action());
}

TEST_F(IsolatedAppThrottleTest, BlockRedirectOutOfIsolatedApp) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kMaybeIsolatedApplication,
            GetWebExposedIsolationLevel(main_frame_id()));

  auto simulator = StartRendererInitiatedNavigation(main_frame_id(), kAppUrl2);

  auto start_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::PROCEED, start_result.action());

  // Redirect to a non-app page.
  simulator->Redirect(GURL(kNonAppUrl));

  auto redirect_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::CANCEL, redirect_result.action());
}

TEST_F(IsolatedAppThrottleTest, AllowIframeNavigationOutOfApp) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kMaybeIsolatedApplication,
            GetWebExposedIsolationLevel(main_frame_id()));
  int iframe_id = CreateIframe(main_frame_id(), "test_frame");

  // Navigate the iframe to an app page.
  CommitRendererInitiatedNavigation(iframe_id, kAppUrl, corp_coep_headers());

  // Navigate the iframe to a non-app page.
  CommitRendererInitiatedNavigation(iframe_id, kNonAppUrl, corp_coep_headers());
}

TEST_F(IsolatedAppThrottleTest,
       BlockIframeRendererInitiatedNavigationIntoIsolatedApp) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kMaybeIsolatedApplication,
            GetWebExposedIsolationLevel(main_frame_id()));
  int iframe_id = CreateIframe(main_frame_id(), "test_frame");

  // Navigate the iframe to a non-app page.
  CommitRendererInitiatedNavigation(iframe_id, kNonAppUrl, corp_coep_headers());

  // Perform a renderer-initiated navigation the iframe to an app page.
  auto simulator = StartRendererInitiatedNavigation(iframe_id, kAppUrl);

  auto start_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::BLOCK_REQUEST, start_result.action());
}

TEST_F(IsolatedAppThrottleTest,
       AllowIframeBrowserInitiatedNavigationIntoIsolatedApp) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kMaybeIsolatedApplication,
            GetWebExposedIsolationLevel(main_frame_id()));
  int iframe_id = CreateIframe(main_frame_id(), "test_frame");

  // Navigate the iframe to an app page.
  CommitRendererInitiatedNavigation(iframe_id, kAppUrl, corp_coep_headers());

  // Navigate the iframe to a non-app page.
  CommitRendererInitiatedNavigation(iframe_id, kNonAppUrl, corp_coep_headers());

  // Go back. We can't use NavigationSimulator::GoBack because we need to set
  // coop headers.
  auto simulator = NavigationSimulator::CreateHistoryNavigation(
      -1, web_contents(), false /* is_renderer_initiated */);
  simulator->Start();

  // Create a new simulator to finish the navigation because the previous one
  // will reset its RenderFrameHost pointer after onunload has run, and will
  // incorrectly choose the main frame as the one being navigated.
  simulator = NavigationSimulatorImpl::CreateFromPendingInFrame(
      FrameTreeNode::GloballyFindByID(iframe_id));
  simulator->SetResponseHeaders(corp_coep_headers());
  simulator->Commit();

  auto commit_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::PROCEED, commit_result.action());
}

TEST_F(IsolatedAppThrottleTest, BlockIframeRedirectOutThenIntoIsolatedApp) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kMaybeIsolatedApplication,
            GetWebExposedIsolationLevel(main_frame_id()));
  int iframe_id = CreateIframe(main_frame_id(), "test_frame");

  auto simulator = StartRendererInitiatedNavigation(iframe_id, kAppUrl);

  auto start_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::PROCEED, start_result.action());

  // Redirect to a non-app page.
  simulator->SetRedirectHeaders(corp_coep_headers());
  simulator->Redirect(GURL(kNonAppUrl));

  auto redirect_result1 = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::PROCEED, redirect_result1.action());
  EXPECT_EQ(GURL(kNonAppUrl), simulator->GetNavigationHandle()->GetURL());

  // Redirect back to an app page.
  simulator->SetRedirectHeaders(corp_coep_headers());
  simulator->Redirect(GURL(kAppUrl2));

  auto redirect_result2 = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::BLOCK_REQUEST, redirect_result2.action());
}

TEST_F(IsolatedAppThrottleTest, BlockIsolatedIframeInNonIsolatedIframe) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kMaybeIsolatedApplication,
            GetWebExposedIsolationLevel(main_frame_id()));

  // Create a non-app iframe.
  int child_iframe_id = CreateIframe(main_frame_id(), "test_frame1");
  CommitRendererInitiatedNavigation(child_iframe_id, kNonAppUrl,
                                    corp_coep_headers());

  // Try to create an app iframe within the non-app iframe.
  int grandchild_iframe_id = CreateIframe(child_iframe_id, "test_frame2");
  auto simulator =
      StartRendererInitiatedNavigation(grandchild_iframe_id, kAppUrl);

  auto start_result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::BLOCK_REQUEST, start_result.action());
}

TEST_F(IsolatedAppThrottleTest, AllowHistoryNavigationFromErrorPage) {
  CommitBrowserInitiatedNavigation(kAppUrl, coop_coep_headers());
  EXPECT_EQ(kMaybeIsolatedApplication,
            GetWebExposedIsolationLevel(main_frame_id()));

  auto* error_rfh = NavigationSimulator::NavigateAndFailFromDocument(
      GURL(kAppUrl2), net::ERR_TIMED_OUT, web_contents()->GetMainFrame());
  EXPECT_NE(nullptr, error_rfh);
  EXPECT_EQ(GURL(kAppUrl2), error_rfh->GetLastCommittedURL());
  EXPECT_EQ(PAGE_TYPE_ERROR, GetPageType(error_rfh));

  // Go back. We can't use NavigationSimulator::GoBack because we need to set
  // coop headers.
  auto simulator = NavigationSimulator::CreateHistoryNavigation(
      -1, web_contents(), false /* is_renderer_initiated */);
  simulator->Start();
  simulator->SetResponseHeaders(coop_coep_headers());
  simulator->Commit();

  auto* app_rfh = simulator->GetFinalRenderFrameHost();
  EXPECT_NE(nullptr, app_rfh);
  EXPECT_EQ(GURL(kAppUrl), app_rfh->GetLastCommittedURL());
  EXPECT_EQ(PAGE_TYPE_NORMAL, GetPageType(app_rfh));
}

}  // namespace content
