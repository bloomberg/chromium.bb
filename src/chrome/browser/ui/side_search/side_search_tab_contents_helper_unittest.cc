// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kSearchMatchUrl1[] = "https://www.search-url-1.com/";
constexpr char kSearchMatchUrl2[] = "https://www.search-url-2.com/";
constexpr char kNonMatchUrl[] = "https://www.tab-frame-url.com/";

bool IsSearchURLMatch(const GURL& url) {
  return url == kSearchMatchUrl1 || url == kSearchMatchUrl2;
}

}  // namespace

namespace test {

class SideSearchTabContentsHelperTest : public ::testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kSideSearch);
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    SideSearchTabContentsHelper::CreateForWebContents(web_contents_.get());
    helper()->SetSidePanelContentsForTesting(
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr));
    // Basic configuration for testing that allows navigations to URLs matching
    // `kSearchMatchUrl1` and `kSearchMatchUrl2` to proceed within the side
    // panel and only allows showing the side panel on non-matching pages.
    auto* config = GetConfig();
    config->SetShouldNavigateInSidePanelCallback(
        base::BindRepeating(IsSearchURLMatch));
    config->SetCanShowSidePanelForURLCallback(base::BindRepeating(
        [](const GURL& url) { return !IsSearchURLMatch(url); }));
    config->SetGenerateSideSearchURLCallback(
        base::BindRepeating([](const GURL& url) { return url; }));
    config->set_is_side_panel_srp_available(true);
    Test::SetUp();
  }

 protected:
  void LoadURL(const char* url) {
    web_contents()->GetController().LoadURL(GURL(url), content::Referrer(),
                                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                            std::string());
    CommitPendingNavigation();
  }

  void GoBack() {
    ASSERT_TRUE(web_contents()->GetController().CanGoBack());
    web_contents()->GetController().GoBack();
    CommitPendingNavigation();
  }

  void GoForward() {
    ASSERT_TRUE(web_contents()->GetController().CanGoForward());
    web_contents()->GetController().GoForward();
    CommitPendingNavigation();
  }

  void CommitPendingNavigation() {
    content::WebContentsTester::For(web_contents())->CommitPendingNavigation();

    if (side_contents() && side_contents()->GetController().GetPendingEntry()) {
      content::WebContentsTester::For(side_contents())
          ->CommitPendingNavigation();
    }
  }

  content::NavigationEntry* GetLastCommittedSideContentsEntry() {
    DCHECK(side_contents());
    return side_contents()->GetController().GetLastCommittedEntry();
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

  content::WebContents* side_contents() {
    return helper()->GetSidePanelContents();
  }

  SideSearchTabContentsHelper* helper() {
    return SideSearchTabContentsHelper::FromWebContents(web_contents_.get());
  }

  SideSearchConfig* GetConfig() { return SideSearchConfig::Get(&profile_); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SideSearchTabContentsHelperTest, LastSearchURLUpdatesCorrectly) {
  // When a tab is first opened there should be no last encountered search URL.
  EXPECT_FALSE(helper()->last_search_url().has_value());
  EXPECT_TRUE(!GetLastCommittedSideContentsEntry() ||
              GetLastCommittedSideContentsEntry()->IsInitialEntry());

  // Navigating to a matching search URL should update the `last_search_url`.
  LoadURL(kSearchMatchUrl1);
  EXPECT_EQ(kSearchMatchUrl1, helper()->last_search_url());
  EXPECT_EQ(kSearchMatchUrl1, GetLastCommittedSideContentsEntry()->GetURL());

  // Navigating to a non-matching search URL should not change the
  // `last_search_url`.
  LoadURL(kNonMatchUrl);
  EXPECT_EQ(kSearchMatchUrl1, helper()->last_search_url());
  EXPECT_EQ(kSearchMatchUrl1, GetLastCommittedSideContentsEntry()->GetURL());

  // Navigating again to a new matching search url should update the
  // `last_search_url`.
  LoadURL(kSearchMatchUrl2);
  EXPECT_EQ(kSearchMatchUrl2, helper()->last_search_url());
  EXPECT_EQ(kSearchMatchUrl2, GetLastCommittedSideContentsEntry()->GetURL());

  // Going backwards to the non-matching search URL should not update the
  // `last_search_url`.
  GoBack();
  EXPECT_EQ(kSearchMatchUrl2, helper()->last_search_url());
  EXPECT_EQ(kSearchMatchUrl2, GetLastCommittedSideContentsEntry()->GetURL());

  // Going back to the original search URL should update the `last_search_url`.
  GoBack();
  EXPECT_EQ(kSearchMatchUrl1, helper()->last_search_url());
  EXPECT_EQ(kSearchMatchUrl1, GetLastCommittedSideContentsEntry()->GetURL());

  // Going forward to the non-matching search URL shouldn't change the
  // `last_search_url`.
  GoForward();
  EXPECT_EQ(kSearchMatchUrl1, helper()->last_search_url());
  EXPECT_EQ(kSearchMatchUrl1, GetLastCommittedSideContentsEntry()->GetURL());

  // Going forward to the latest matching search URL should update the
  // `last_search_url` to that latest URL.
  GoForward();
  EXPECT_EQ(kSearchMatchUrl2, helper()->last_search_url());
  EXPECT_EQ(kSearchMatchUrl2, GetLastCommittedSideContentsEntry()->GetURL());
}

TEST_F(SideSearchTabContentsHelperTest, IndicatesWhenSidePanelShouldBeShown) {
  // With no initial navigation the side panel should not be showing.
  EXPECT_FALSE(helper()->CanShowSidePanelForCommittedNavigation());

  // If no previous matching search URL has been seen for this tab contents the
  // side panel should not show.
  LoadURL(kNonMatchUrl);
  EXPECT_FALSE(helper()->CanShowSidePanelForCommittedNavigation());

  // The side panel should not be visible on matching search pages.
  LoadURL(kSearchMatchUrl1);
  EXPECT_FALSE(helper()->CanShowSidePanelForCommittedNavigation());

  // If a matching page has previously been seen the side panel may be opened
  // on non-matching pages.
  LoadURL(kNonMatchUrl);
  EXPECT_TRUE(helper()->CanShowSidePanelForCommittedNavigation());
}

TEST_F(SideSearchTabContentsHelperTest, ClearsSidePanelContentsWhenAsked) {
  EXPECT_NE(nullptr, helper()->side_panel_contents_for_testing());
  helper()->ClearSidePanelContents();
  EXPECT_EQ(nullptr, helper()->side_panel_contents_for_testing());
}

TEST_F(SideSearchTabContentsHelperTest,
       SidePanelProcessGoneResetsSideContents) {
  EXPECT_NE(nullptr, helper()->side_panel_contents_for_testing());
  helper()->SidePanelProcessGone();
  EXPECT_EQ(nullptr, helper()->side_panel_contents_for_testing());
}

TEST_F(SideSearchTabContentsHelperTest, ClearsInternalStateWhenConfigChanges) {
  // When a tab is first opened there should be no last encountered search URL.
  EXPECT_FALSE(helper()->last_search_url().has_value());
  EXPECT_TRUE(!GetLastCommittedSideContentsEntry() ||
              GetLastCommittedSideContentsEntry()->IsInitialEntry());

  // Navigate to a search URL.
  LoadURL(kSearchMatchUrl1);
  EXPECT_EQ(kSearchMatchUrl1, helper()->last_search_url());
  EXPECT_EQ(kSearchMatchUrl1, GetLastCommittedSideContentsEntry()->GetURL());

  // Set the toggled bit to true.
  helper()->set_toggled_open(true);

  // Reset config state and notify observers that the config has changed.
  GetConfig()->ResetStateAndNotifyConfigChanged();

  // Check to make sure state has been reset.
  EXPECT_FALSE(helper()->last_search_url().has_value());
  EXPECT_FALSE(helper()->toggled_open());
  EXPECT_EQ(nullptr, helper()->side_panel_contents_for_testing());
}

}  // namespace test
