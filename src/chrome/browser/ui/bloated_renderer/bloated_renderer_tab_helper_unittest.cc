// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bloated_renderer/bloated_renderer_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/page_importance_signals.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class BloatedRendererTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    BloatedRendererTabHelper::CreateForWebContents(web_contents());
    tab_helper_ = BloatedRendererTabHelper::FromWebContents(web_contents());
    web_contents_tester_ = content::WebContentsTester::For(web_contents());
    web_contents_tester_->SetLastCommittedURL(GURL("https://test.test"));
  }
  BloatedRendererTabHelper* tab_helper_;
  content::WebContentsTester* web_contents_tester_;
};

TEST_F(BloatedRendererTabHelperTest, DetectReload) {
  EXPECT_EQ(BloatedRendererTabHelper::State::kInactive, tab_helper_->state_);
  tab_helper_->state_ = BloatedRendererTabHelper::State::kRequestingReload;
  auto reload_navigation =
      content::NavigationHandle::CreateNavigationHandleForTesting(
          GURL(), web_contents()->GetMainFrame());
  tab_helper_->DidStartNavigation(reload_navigation.get());
  EXPECT_EQ(BloatedRendererTabHelper::State::kStartedNavigation,
            tab_helper_->state_);
  EXPECT_EQ(reload_navigation->GetNavigationId(),
            tab_helper_->saved_navigation_id_);
  tab_helper_->DidFinishNavigation(reload_navigation.get());
  EXPECT_EQ(BloatedRendererTabHelper::State::kInactive, tab_helper_->state_);
}

TEST_F(BloatedRendererTabHelperTest, IgnoreUnrelatedNavigation) {
  EXPECT_EQ(BloatedRendererTabHelper::State::kInactive, tab_helper_->state_);
  tab_helper_->state_ = BloatedRendererTabHelper::State::kRequestingReload;
  auto reload_navigation =
      content::NavigationHandle::CreateNavigationHandleForTesting(
          GURL(), web_contents()->GetMainFrame());
  tab_helper_->DidStartNavigation(reload_navigation.get());
  EXPECT_EQ(BloatedRendererTabHelper::State::kStartedNavigation,
            tab_helper_->state_);
  EXPECT_EQ(reload_navigation->GetNavigationId(),
            tab_helper_->saved_navigation_id_);
  auto unrelated_navigation =
      content::NavigationHandle::CreateNavigationHandleForTesting(
          GURL(), web_contents()->GetMainFrame());
  tab_helper_->DidFinishNavigation(unrelated_navigation.get());
  EXPECT_EQ(BloatedRendererTabHelper::State::kStartedNavigation,
            tab_helper_->state_);
  EXPECT_EQ(reload_navigation->GetNavigationId(),
            tab_helper_->saved_navigation_id_);
}

TEST_F(BloatedRendererTabHelperTest, CanReloadBloatedTab) {
  web_contents_tester_->NavigateAndCommit(GURL("https://test.test"));
  EXPECT_TRUE(tab_helper_->CanReloadBloatedTab());
}

TEST_F(BloatedRendererTabHelperTest, CannotReloadBloatedTabCrashed) {
  web_contents()->SetIsCrashed(base::TERMINATION_STATUS_PROCESS_CRASHED, 0);

  EXPECT_FALSE(tab_helper_->CanReloadBloatedTab());
}

TEST_F(BloatedRendererTabHelperTest, CannotReloadBloatedTabInvalidURL) {
  web_contents_tester_->SetLastCommittedURL(GURL("invalid :)"));

  EXPECT_FALSE(tab_helper_->CanReloadBloatedTab());
}

TEST_F(BloatedRendererTabHelperTest, CannotReloadBloatedTabWithPostData) {
  web_contents_tester_->NavigateAndCommit(GURL("https://test.test"));
  web_contents()->GetController().GetLastCommittedEntry()->SetHasPostData(true);

  EXPECT_FALSE(tab_helper_->CanReloadBloatedTab());
}

TEST_F(BloatedRendererTabHelperTest,
       CannotReloadBloatedTabPendingUserInteraction) {
  content::PageImportanceSignals signals;
  signals.had_form_interaction = true;
  web_contents_tester_->SetPageImportanceSignals(signals);
  EXPECT_FALSE(tab_helper_->CanReloadBloatedTab());
}
