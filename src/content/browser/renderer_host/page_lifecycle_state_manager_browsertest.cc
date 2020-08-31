// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class PageLifecycleStateManagerBrowserTest : public ContentBrowserTest {
 public:
  ~PageLifecycleStateManagerBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  void StartRecordingEvents(RenderFrameHostImpl* rfh) {
    EXPECT_TRUE(ExecJs(rfh, R"(
      window.testObservedEvents = [];
      let event_list = [
        'freeze',
        'resume',
        'visibilitychange',
      ];
      for (event_name of event_list) {
        let result = event_name;
        document.addEventListener(event_name, event => {
          window.testObservedEvents.push('document.' + result);
        });
      }
    )"));
  }

  void MatchEventList(RenderFrameHostImpl* rfh,
                      base::ListValue list,
                      base::Location location = base::Location::Current()) {
    EXPECT_EQ(list, EvalJs(rfh, "window.testObservedEvents"))
        << location.ToString();
  }

  RenderViewHostImpl* render_view_host() {
    return static_cast<RenderViewHostImpl*>(
        shell()->web_contents()->GetRenderViewHost());
  }

  RenderFrameHostImpl* current_frame_host() {
    return static_cast<WebContentsImpl*>(shell()->web_contents())
        ->GetFrameTree()
        ->root()
        ->current_frame_host();
  }
};

IN_PROC_BROWSER_TEST_F(PageLifecycleStateManagerBrowserTest, SetFrozen) {
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHostImpl* rfh = current_frame_host();
  StartRecordingEvents(rfh);

  // Hide and freeze the page.
  shell()->web_contents()->WasHidden();
  EXPECT_EQ(PageVisibilityState::kHidden, rfh->GetVisibilityState());
  shell()->web_contents()->SetPageFrozen(true);

  // Resume the page.
  shell()->web_contents()->SetPageFrozen(false);

  MatchEventList(rfh, ListValueOf("document.visibilitychange",
                                  "document.freeze", "document.resume"));
}

IN_PROC_BROWSER_TEST_F(PageLifecycleStateManagerBrowserTest, SetVisibility) {
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHostImpl* rfh = current_frame_host();
  StartRecordingEvents(rfh);

  // Hide the page.
  shell()->web_contents()->WasHidden();
  EXPECT_EQ(PageVisibilityState::kHidden, rfh->GetVisibilityState());

  MatchEventList(rfh, ListValueOf("document.visibilitychange"));
}

IN_PROC_BROWSER_TEST_F(PageLifecycleStateManagerBrowserTest,
                       CrossProcessIframeHiddenAnFrozen) {
  EXPECT_TRUE(embedded_test_server()->Start());
  // Load a page with a cross-process iframe.
  GURL url_a_b(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a_b));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  StartRecordingEvents(rfh_a);
  StartRecordingEvents(rfh_b);

  // Hide and freeze the page.
  shell()->web_contents()->WasHidden();
  EXPECT_EQ(PageVisibilityState::kHidden, rfh_a->GetVisibilityState());
  EXPECT_EQ(PageVisibilityState::kHidden, rfh_b->GetVisibilityState());

  shell()->web_contents()->SetPageFrozen(true);

  // Resume the page.
  shell()->web_contents()->SetPageFrozen(false);

  // Make sure that the cross-process iframe also got events.
  MatchEventList(rfh_a, ListValueOf("document.visibilitychange",
                                    "document.freeze", "document.resume"));
  MatchEventList(rfh_b, ListValueOf("document.visibilitychange",
                                    "document.freeze", "document.resume"));
}

IN_PROC_BROWSER_TEST_F(PageLifecycleStateManagerBrowserTest,
                       CreateIframeInHiddenPage) {
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/empty.html");

  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHostImpl* rfh_parent = current_frame_host();

  // Hide the page.
  shell()->web_contents()->WasHidden();
  EXPECT_EQ(PageVisibilityState::kHidden, rfh_parent->GetVisibilityState());

  // Create an iframe in a hidden page.
  EXPECT_TRUE(ExecJs(rfh_parent, R"(
    let iframe = document.createElement('iframe');
    document.body.append(iframe);
  )"));
  ASSERT_EQ(1u, rfh_parent->child_count());

  // Make sure that the created iframe's initial visibility is correctly set.
  RenderFrameHostImpl* rfh_child =
      rfh_parent->child_at(0)->current_frame_host();
  EXPECT_EQ(PageVisibilityState::kHidden, rfh_child->GetVisibilityState());

  // Show the page.
  shell()->web_contents()->WasShown();
  EXPECT_EQ(PageVisibilityState::kVisible, rfh_parent->GetVisibilityState());
  EXPECT_EQ(PageVisibilityState::kVisible, rfh_child->GetVisibilityState());
}

}  // namespace content
