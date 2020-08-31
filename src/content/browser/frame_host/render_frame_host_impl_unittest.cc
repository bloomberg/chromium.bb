// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "content/browser/frame_host/render_frame_host_impl.h"
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

  // Start the test with a simple navigation.
  {
    std::unique_ptr<NavigationSimulator> simulator =
        NavigationSimulator::CreateRendererInitiated(initial_url, main_rfh());
    simulator->Start();
    simulator->Commit();
  }
  RenderFrameHost* initial_rfh = main_rfh();

  // Verify expected main world origin in a steady state - after a commit it
  // should be the same as the last committed origin.
  EXPECT_EQ(url::Origin::Create(initial_url),
            static_cast<RenderFrameHostImpl*>(main_rfh())
                ->GetExpectedMainWorldOriginForUrlLoaderFactory());
  EXPECT_EQ(url::Origin::Create(initial_url),
            main_rfh()->GetLastCommittedOrigin());

  // Verify expected main world origin when a pending navigation was started but
  // hasn't yet reached the ready-to-commit state.
  std::unique_ptr<NavigationSimulator> simulator2 =
      NavigationSimulator::CreateRendererInitiated(final_url, main_rfh());
  simulator2->Start();
  EXPECT_EQ(url::Origin::Create(initial_url),
            static_cast<RenderFrameHostImpl*>(main_rfh())
                ->GetExpectedMainWorldOriginForUrlLoaderFactory());

  // Verify expected main world origin when a pending navigation has reached the
  // ready-to-commit state.  Note that the last committed origin shouldn't
  // change yet at this point.
  simulator2->ReadyToCommit();
  simulator2->Wait();
  EXPECT_EQ(url::Origin::Create(final_url),
            static_cast<RenderFrameHostImpl*>(main_rfh())
                ->GetExpectedMainWorldOriginForUrlLoaderFactory());
  EXPECT_EQ(url::Origin::Create(initial_url),
            main_rfh()->GetLastCommittedOrigin());

  // Verify expected main world origin once we are again in a steady state -
  // after a commit.
  simulator2->Commit();
  EXPECT_EQ(url::Origin::Create(final_url),
            static_cast<RenderFrameHostImpl*>(main_rfh())
                ->GetExpectedMainWorldOriginForUrlLoaderFactory());
  EXPECT_EQ(url::Origin::Create(final_url),
            main_rfh()->GetLastCommittedOrigin());

  // As a test sanity check, verify that there was no RFH swap (the bug this
  // test protects against would only happen if there is no swap).  In fact,
  // GetExpectedMainWorldOriginForUrlLoaderFactory can be removed entirely once
  // we swap on all document changes.
  EXPECT_EQ(initial_rfh, main_rfh());
}

// Create a full screen popup RenderWidgetHost and View.
TEST_F(RenderFrameHostImplTest, CreateFullscreenWidget) {
  mojo::PendingRemote<mojom::Widget> widget;
  std::unique_ptr<MockWidgetImpl> widget_impl =
      std::make_unique<MockWidgetImpl>(widget.InitWithNewPipeAndPassReceiver());
  main_test_rfh()->CreateNewFullscreenWidget(
      std::move(widget),
      mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>(),
      mojo::PendingAssociatedRemote<blink::mojom::Widget>(), base::DoNothing());
}

}  // namespace content
