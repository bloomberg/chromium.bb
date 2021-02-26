// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/test_utils.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class RenderFrameHostImplTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
  }
};

TEST_F(RenderFrameHostImplTest, ExpectedMainWorldOrigin) {
  GURL initial_url = GURL("https://initial.example.test/");
  GURL final_url = GURL("https://final.example.test/");

  auto get_expected_main_world_origin = [](RenderFrameHost* rfh) {
    NavigationRequest* in_flight_request =
        static_cast<RenderFrameHostImpl*>(rfh)
            ->FindLatestNavigationRequestThatIsStillCommitting();

    return in_flight_request ? in_flight_request->GetOriginForURLLoaderFactory()
                             : rfh->GetLastCommittedOrigin();
  };

  // Start the test with a simple navigation.
  {
    std::unique_ptr<NavigationSimulator> simulator =
        NavigationSimulator::CreateRendererInitiated(initial_url, main_rfh());
    simulator->Start();
    simulator->Commit();
  }
  RenderFrameHost* initial_rfh = main_rfh();
  // This test is for a bug that only happens when there is no RFH swap on
  // same-site navigations, so we should disable same-site proactive
  // BrowsingInstance for |initial_rfh| before continiung.
  // Note: this will not disable RenderDocument.
  // TODO(crbug.com/936696): Skip this test when main-frame RenderDocument is
  // enabled.
  DisableProactiveBrowsingInstanceSwapFor(initial_rfh);
  // Verify expected main world origin in a steady state - after a commit it
  // should be the same as the last committed origin.
  EXPECT_EQ(url::Origin::Create(initial_url),
            get_expected_main_world_origin(main_rfh()));
  EXPECT_EQ(url::Origin::Create(initial_url),
            main_rfh()->GetLastCommittedOrigin());

  // Verify expected main world origin when a pending navigation was started but
  // hasn't yet reached the ready-to-commit state.
  std::unique_ptr<NavigationSimulator> simulator2 =
      NavigationSimulator::CreateRendererInitiated(final_url, main_rfh());
  simulator2->Start();
  EXPECT_EQ(url::Origin::Create(initial_url),
            get_expected_main_world_origin(main_rfh()));

  // Verify expected main world origin when a pending navigation has reached the
  // ready-to-commit state.  Note that the last committed origin shouldn't
  // change yet at this point.
  simulator2->ReadyToCommit();
  simulator2->Wait();
  EXPECT_EQ(url::Origin::Create(final_url),
            get_expected_main_world_origin(main_rfh()));
  EXPECT_EQ(url::Origin::Create(initial_url),
            main_rfh()->GetLastCommittedOrigin());

  // Verify expected main world origin once we are again in a steady state -
  // after a commit.
  simulator2->Commit();
  EXPECT_EQ(url::Origin::Create(final_url),
            get_expected_main_world_origin(main_rfh()));
  EXPECT_EQ(url::Origin::Create(final_url),
            main_rfh()->GetLastCommittedOrigin());

  // As a test sanity check, verify that there was no RFH swap (the bug this
  // test protects against would only happen if there is no swap).  In fact,
  // FindLatestNavigationRequestThatIsStillCommitting might possibly be removed
  // entirely once we swap on all document changes.
  EXPECT_EQ(initial_rfh, main_rfh());
}

TEST_F(RenderFrameHostImplTest, PolicyContainerLifecycle) {
  TestRenderFrameHost* main_rfh = contents()->GetMainFrame();
  ASSERT_NE(main_rfh->policy_container_host(), nullptr);
  EXPECT_EQ(main_rfh->policy_container_host()->referrer_policy(),
            network::mojom::ReferrerPolicy::kDefault);

  static_cast<blink::mojom::PolicyContainerHost*>(
      main_rfh->policy_container_host())
      ->SetReferrerPolicy(network::mojom::ReferrerPolicy::kAlways);
  EXPECT_EQ(main_rfh->policy_container_host()->referrer_policy(),
            network::mojom::ReferrerPolicy::kAlways);

  // Create a child frame and check that it inherits the PolicyContainerHost
  // from the parent frame.
  auto* child_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_test_rfh())
          ->AppendChild("child"));

  ASSERT_NE(child_frame->policy_container_host(), nullptr);
  EXPECT_EQ(child_frame->policy_container_host()->referrer_policy(),
            network::mojom::ReferrerPolicy::kAlways);

  // Create a new WebContents with opener and test that the new main frame
  // inherits the PolicyContainerHost from the opener.
  static_cast<blink::mojom::PolicyContainerHost*>(
      child_frame->policy_container_host())
      ->SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  WebContents::CreateParams params(browser_context());
  std::unique_ptr<WebContentsImpl> new_contents(
      WebContentsImpl::CreateWithOpener(params, child_frame));
  RenderFrameHostImpl* new_frame =
      new_contents->GetFrameTree()->root()->current_frame_host();

  ASSERT_NE(new_frame->policy_container_host(), nullptr);
  EXPECT_EQ(new_frame->policy_container_host()->referrer_policy(),
            network::mojom::ReferrerPolicy::kNever);
}

}  // namespace content
