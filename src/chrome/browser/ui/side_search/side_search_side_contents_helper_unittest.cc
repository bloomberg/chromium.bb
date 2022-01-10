// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_side_contents_helper.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/side_search/side_search_metrics.h"
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

constexpr char kGoogleSearchURL[] = "https://www.google.com/search?q=test1";
constexpr char kGoogleSearchHomePageURL[] = "https://www.google.com";
constexpr char kNonGoogleURL[] = "https://www.test.com";

class MockSideContentsDelegate : public SideSearchSideContentsHelper::Delegate {
 public:
  MockSideContentsDelegate() = default;
  MockSideContentsDelegate(const MockSideContentsDelegate&) = delete;
  MockSideContentsDelegate& operator=(const MockSideContentsDelegate&) = delete;
  ~MockSideContentsDelegate() = default;

  // SideSearchSideContentsHelper::Delegate:
  void NavigateInTabContents(const content::OpenURLParams& params) override {
    tab_contents_url_ = params.url;
    ++navigate_in_tab_contents_times_called_;
  }
  void LastSearchURLUpdated(const GURL& url) override {
    last_search_url_ = url;
  }
  void SidePanelProcessGone() override {}

  const GURL& tab_contents_url() const { return tab_contents_url_; }

  const GURL& last_search_url() const { return last_search_url_; }

  int navigate_in_tab_contents_times_called() const {
    return navigate_in_tab_contents_times_called_;
  }

 private:
  GURL tab_contents_url_;
  GURL last_search_url_;
  int navigate_in_tab_contents_times_called_ = 0;
};

}  // namespace

namespace test {

class SideSearchSideContentsHelperTest : public ::testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kSideSearch);
    side_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    SideSearchSideContentsHelper::CreateForWebContents(side_contents());
    helper()->SetDelegate(&delegate_);
    Test::SetUp();
  }

 protected:
  void LoadURL(const char* url) {
    const int initial_tab_navigation_count =
        delegate_.navigate_in_tab_contents_times_called();
    side_contents()->GetController().LoadURL(GURL(url), content::Referrer(),
                                             ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                             std::string());
    const int final_tab_navigation_count =
        delegate_.navigate_in_tab_contents_times_called();

    // If NavigateInTabContents() has been called this indicates that the side
    // contents' navigation has been cancelled by the throttle. Avoid committing
    // the side contents' navigation in this case as it will trigger a DCHECK().
    if (side_contents()->GetController().GetPendingEntry() &&
        initial_tab_navigation_count == final_tab_navigation_count) {
      content::WebContentsTester::For(side_contents())
          ->CommitPendingNavigation();
    }
  }

  content::NavigationEntry* GetLastCommittedSideContentsEntry() {
    return side_contents()->GetController().GetLastCommittedEntry();
  }

  const MockSideContentsDelegate& delegate() { return delegate_; }

  content::WebContents* side_contents() { return side_contents_.get(); }

  SideSearchSideContentsHelper* helper() {
    return SideSearchSideContentsHelper::FromWebContents(side_contents());
  }

  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> side_contents_;
  MockSideContentsDelegate delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SideSearchSideContentsHelperTest, GoogleSearchURLsNavigateSideContents) {
  LoadURL(kGoogleSearchURL);
  EXPECT_EQ(GURL(kGoogleSearchURL),
            GetLastCommittedSideContentsEntry()->GetURL());
  EXPECT_EQ(GURL(kGoogleSearchURL), delegate().last_search_url());
  EXPECT_TRUE(delegate().tab_contents_url().is_empty());
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.Navigation",
      SideSearchNavigationType::kNavigationCommittedWithinSideSearch, 1);
}

TEST_F(SideSearchSideContentsHelperTest,
       NonGoogleSearchURLNavigatesTabContents) {
  LoadURL(kNonGoogleURL);
  EXPECT_TRUE(GetLastCommittedSideContentsEntry()->IsInitialEntry());
  EXPECT_TRUE(delegate().last_search_url().is_empty());
  EXPECT_EQ(GURL(kNonGoogleURL), delegate().tab_contents_url());
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.Navigation", SideSearchNavigationType::kRedirectionToTab, 1);
}

TEST_F(SideSearchSideContentsHelperTest,
       GoogleHomePageURLNavigatesTabContents) {
  LoadURL(kGoogleSearchHomePageURL);
  EXPECT_TRUE(GetLastCommittedSideContentsEntry()->IsInitialEntry());
  EXPECT_TRUE(delegate().last_search_url().is_empty());
  EXPECT_EQ(GURL(kGoogleSearchHomePageURL), delegate().tab_contents_url());
  histogram_tester_.ExpectUniqueSample(
      "SideSearch.Navigation", SideSearchNavigationType::kRedirectionToTab, 1);
}

}  // namespace test
