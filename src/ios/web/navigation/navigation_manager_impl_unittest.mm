// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_manager_impl.h"

#include <array>
#include <string>

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/navigation/wk_based_navigation_manager_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/web_client.h"
#import "ios/web/test/fakes/crw_fake_back_forward_list.h"
#include "ios/web/test/test_url_constants.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "url/scheme_host_port.h"
#include "url/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace web {
namespace {

// URL scheme that will be rewritten by UrlRewriter installed in
// NavigationManagerTest fixture. Scheme will be changed to kTestWebUIScheme.
const char kSchemeToRewrite[] = "navigationmanagerschemetorewrite";

// Replaces |kSchemeToRewrite| scheme with |kTestWebUIScheme|.
bool UrlRewriter(GURL* url, BrowserState* browser_state) {
  if (url->scheme() == kSchemeToRewrite) {
    GURL::Replacements scheme_replacements;
    scheme_replacements.SetSchemeStr(kTestWebUIScheme);
    *url = url->ReplaceComponents(scheme_replacements);
  }
  return false;
}

// Query parameter that will be appended by AppendingUrlRewriter if it is
// installed into NavigationManager by a test case.
const char kRewrittenQueryParam[] = "navigationmanagerrewrittenquery";

// Appends |kRewrittenQueryParam| to |url|.
bool AppendingUrlRewriter(GURL* url, BrowserState* browser_state) {
  GURL::Replacements query_replacements;
  query_replacements.SetQueryStr(kRewrittenQueryParam);
  *url = url->ReplaceComponents(query_replacements);
  return false;
}

// Mock class for NavigationManagerDelegate.
class MockNavigationManagerDelegate : public NavigationManagerDelegate {
 public:
  void SetWKWebView(id web_view) { mock_web_view_ = web_view; }
  void SetWebState(WebState* web_state) { web_state_ = web_state; }

  MOCK_METHOD0(ClearTransientContent, void());
  MOCK_METHOD0(ClearDialogs, void());
  MOCK_METHOD0(RecordPageStateInNavigationItem, void());
  MOCK_METHOD2(OnGoToIndexSameDocumentNavigation,
               void(NavigationInitiationType type, bool has_user_gesture));
  MOCK_METHOD1(LoadCurrentItem, void(NavigationInitiationType type));
  MOCK_METHOD0(LoadIfNecessary, void());
  MOCK_METHOD0(Reload, void());
  MOCK_METHOD1(OnNavigationItemCommitted, void(NavigationItem* item));
  MOCK_METHOD0(RemoveWebView, void());
  MOCK_METHOD4(GoToBackForwardListItem,
               void(WKBackForwardListItem*,
                    NavigationItem*,
                    NavigationInitiationType,
                    bool));
  MOCK_METHOD0(GetPendingItem, NavigationItemImpl*());

 private:
  WebState* GetWebState() override { return web_state_; }

  id<CRWWebViewNavigationProxy> GetWebViewNavigationProxy() const override {
    return mock_web_view_;
  }

  id mock_web_view_;
  WebState* web_state_ = nullptr;
};

// Data holder for the informations to be restored in the items.
struct ItemInfoToBeRestored {
  GURL url;
  GURL virtual_url;
  UserAgentType user_agent;
  PageDisplayState display_state;
};

}  // namespace

// Test fixture for NavigationManagerImpl testing.
class NavigationManagerTest : public PlatformTest {
 protected:
  NavigationManagerTest() {
    manager_.reset(new WKBasedNavigationManagerImpl);
    mock_web_view_ = OCMClassMock([WKWebView class]);
    mock_wk_list_ = [[CRWFakeBackForwardList alloc] init];
    OCMStub([mock_web_view_ backForwardList]).andReturn(mock_wk_list_);
    delegate_.SetWKWebView(mock_web_view_);

    // Setup rewriter.
    BrowserURLRewriter::GetInstance()->AddURLRewriter(UrlRewriter);
    url::AddStandardScheme(kSchemeToRewrite, url::SCHEME_WITH_HOST);

    manager_->SetDelegate(&delegate_);
    manager_->SetBrowserState(&browser_state_);
  }

  NavigationManagerImpl* navigation_manager() { return manager_.get(); }

  MockNavigationManagerDelegate& navigation_manager_delegate() {
    return delegate_;
  }

  // Manipulates the underlying session state to simulate the effect of
  // GoToIndex() on the navigation manager to facilitate testing of other
  // NavigationManager APIs. NavigationManager::GoToIndex() itself is tested
  // separately by verifying expectations on the delegate method calls.
  void SimulateGoToIndex(int index) {
    [mock_wk_list_ moveCurrentToIndex:index];
  }

  // Makes delegate to return navigation item, which is stored in navigation
  // context in the real app.
  void SimulateReturningPendingItemFromDelegate(web::NavigationItemImpl* item) {
      ON_CALL(navigation_manager_delegate(), GetPendingItem())
          .WillByDefault(testing::Return(item));
  }

  CRWFakeBackForwardList* mock_wk_list_;
  WKWebView* mock_web_view_;
  base::HistogramTester histogram_tester_;

 protected:
  TestBrowserState browser_state_;
  TestWebState web_state_;
  MockNavigationManagerDelegate delegate_;
  std::unique_ptr<NavigationManagerImpl> manager_;

 private:
  url::ScopedSchemeRegistryForTests scoped_registry_;
};

// Tests state of an empty navigation manager.
TEST_F(NavigationManagerTest, EmptyManager) {
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
  EXPECT_EQ(-1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(0));
}

// Tests that GetPendingItemIndex() returns -1 if there is no pending entry.
TEST_F(NavigationManagerTest, GetPendingItemIndexWithoutPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
}

// Tests that GetPendingItemIndex() returns -1 if there is a pending item.
TEST_F(NavigationManagerTest, GetPendingItemIndexWithPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
}

// Tests that setting and getting PendingItemIndex.
TEST_F(NavigationManagerTest, SetAndGetPendingItemIndex) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.test"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.test"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->SetPendingItemIndex(0);
  EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
}

// Tests that GetPendingItemIndex() returns correct index.
TEST_F(NavigationManagerTest, GetPendingItemIndexWithIndexedPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                  backListURLs:@[ @"http://www.url.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
}

// Tests that NavigationManagerImpl::GetPendingItem() returns item provided by
// the delegate.
TEST_F(NavigationManagerTest, GetPendingItemFromDelegate) {
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  auto item = std::make_unique<web::NavigationItemImpl>();
  SimulateReturningPendingItemFromDelegate(item.get());
  EXPECT_EQ(item.get(), navigation_manager()->GetPendingItem());
}

// Tests that NavigationManagerImpl::GetPendingItem() ignores item provided by
// the delegate if navigation manager has own pending item.
TEST_F(NavigationManagerTest, GetPendingItemIgnoringDelegate) {
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  auto item = std::make_unique<web::NavigationItemImpl>();
  SimulateReturningPendingItemFromDelegate(item.get());

  GURL url("http://www.url.test");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_NE(item.get(), navigation_manager()->GetPendingItem());
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(url, navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that GetPendingItem() returns indexed pending item.
TEST_F(NavigationManagerTest, GetPendingItemWithIndexedPendingEntry) {
  GURL url("http://www.url.test");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.test"];
  navigation_manager()->CommitPendingItem();
  navigation_manager()->SetPendingItemIndex(0);

  auto item = std::make_unique<web::NavigationItemImpl>();
  SimulateReturningPendingItemFromDelegate(item.get());

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_NE(item.get(), navigation_manager()->GetPendingItem());
  EXPECT_EQ(url, navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that going back or negative offset is not possible without a committed
// item.
TEST_F(NavigationManagerTest, CanGoBackWithoutCommitedItem) {
  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is not possible if there is a
// transient item, but not committed items.
TEST_F(NavigationManagerTest, CanGoBackWithTransientItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com"));

  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is possible if there is a transient
// item and at least one committed item.
TEST_F(NavigationManagerTest, CanGoBackWithTransientItemAndCommittedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  // Setup a pending item for which a transient item can be added.
  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/0"));

  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is not possible if there is ony one
// committed item and no transient item.
TEST_F(NavigationManagerTest, CanGoBackWithSingleCommitedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests going back possibility with multiple committed items.
TEST_F(NavigationManagerTest, CanGoBackWithMultipleCommitedItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                  backListURLs:@[ @"http://www.url.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_
        setCurrentURL:@"http://www.url.com/1"
         backListURLs:@[ @"http://www.url.com", @"http://www.url.com/0" ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));

  SimulateGoToIndex(1);
  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));

  SimulateGoToIndex(0);
  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));

  SimulateGoToIndex(1);
  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going forward or positive offset is not possible without a
// committed item.
TEST_F(NavigationManagerTest, CanGoForwardWithoutCommitedItem) {
  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests that going forward or positive offset is not possible if there is ony
// one committed item and no transient item.
TEST_F(NavigationManagerTest, CanGoForwardWithSingleCommitedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/"];
  navigation_manager()->CommitPendingItem();

  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests going forward possibility with multiple committed items.
TEST_F(NavigationManagerTest, CanGoForwardWithMultipleCommitedEntries) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                  backListURLs:@[ @"http://www.url.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_
        setCurrentURL:@"http://www.url.com/1"
         backListURLs:@[ @"http://www.url.com", @"http://www.url.com/0" ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));

  SimulateGoToIndex(1);
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  SimulateGoToIndex(0);
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  SimulateGoToIndex(1);
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  SimulateGoToIndex(2);
  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests CanGoToOffset API for positive, negative and zero delta. Tested
// navigation manager will have redirect entries to make sure they are
// appropriately skipped.
TEST_F(NavigationManagerTest, OffsetsWithoutPendingIndex) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/redirect"), Referrer(),
      ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/redirect"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[
                    @"http://www.url.com/0", @"http://www.url.com/redirect"
                  ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/2"
                  backListURLs:@[
                    @"http://www.url.com/0", @"http://www.url.com/redirect",
                    @"http://www.url.com/1"
                  ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/redirect"), Referrer(),
      ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/redirect"
                  backListURLs:@[
                    @"http://www.url.com/0", @"http://www.url.com/redirect",
                    @"http://www.url.com/1", @"http://www.url.com/2"
                  ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
}

// Tests that when given a pending item, adding a new pending item replaces the
// existing pending item if their URLs are different.
TEST_F(NavigationManagerTest, ReplacePendingItemIfDiffernetURL) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(existing_url, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  GURL new_url = GURL("http://www.new.com");
  navigation_manager()->AddPendingItem(
      new_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(new_url, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL doesn't replace the existing pending item if new pending item is not a
// form submission.
TEST_F(NavigationManagerTest, NotReplaceSameUrlPendingItemIfNotFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());

    // WKBasedNavigationManager assumes that AddPendingItem() is only called for
    // new navigation, so it always creates a new pending item.
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_manager()->GetPendingItem()->GetTransitionType(),
        ui::PAGE_TRANSITION_LINK));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL replaces the existing pending item if new pending item is a form
// submission while existing pending item is not.
TEST_F(NavigationManagerTest, ReplaceSameUrlPendingItemIfFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_FORM_SUBMIT));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL doesn't replace the existing pending item if the user agent override
// option is INHERIT.
TEST_F(NavigationManagerTest, NotReplaceSameUrlPendingItemIfOverrideInherit) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());

    // WKBasedNavigationManager assumes that AddPendingItem() is only called for
    // new navigation, so it always creates a new pending item.
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_manager()->GetPendingItem()->GetTransitionType(),
        ui::PAGE_TRANSITION_LINK));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL replaces the existing pending item if the user agent override option is
// DESKTOP.
TEST_F(NavigationManagerTest, ReplaceSameUrlPendingItemIfOverrideDesktop) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetPendingItem()->GetUserAgentType(nil));
  EXPECT_EQ(
      web::UserAgentType::MOBILE,
      navigation_manager()->GetPendingItem()->GetUserAgentForInheritance());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL replaces the existing pending item if the user agent override option is
// MOBILE.
TEST_F(NavigationManagerTest, ReplaceSameUrlPendingItemIfOverrideMobile) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::UserAgentType::DESKTOP,
            navigation_manager()->GetPendingItem()->GetUserAgentType(nil));
  EXPECT_EQ(
      web::UserAgentType::DESKTOP,
      navigation_manager()->GetPendingItem()->GetUserAgentForInheritance());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item
// succeeds if the new item's URL is different from the last committed item.
TEST_F(NavigationManagerTest, AddPendingItemIfDiffernetURL) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(existing_url,
            navigation_manager()->GetLastCommittedItem()->GetURL());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  GURL new_url = GURL("http://www.new.com");
  navigation_manager()->AddPendingItem(
      new_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(new_url, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL fails if the new item is not form submission.
TEST_F(NavigationManagerTest, NotAddSameUrlPendingItemIfNotFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

    // WKBasedNavigationManager assumes that AddPendingItem() is only called for
    // new navigation, so it always creates a new pending item.
    ASSERT_TRUE(navigation_manager()->GetPendingItem());
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_manager()->GetPendingItem()->GetTransitionType(),
        ui::PAGE_TRANSITION_LINK));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL succeeds if the new item is a form submission while the last
// committed item is not.
TEST_F(NavigationManagerTest, AddSameUrlPendingItemIfFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  // Add if new transition is a form submission.
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_FORM_SUBMIT));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL fails if the user agent override option is INHERIT.
TEST_F(NavigationManagerTest, NotAddSameUrlPendingItemIfOverrideInherit) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  // WKBasedNavigationManager assumes that AddPendingItem() is only called for
  // new navigation, so it always creates a new pending item.
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_LINK));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL succeeds if the user agent override option is DESKTOP.
TEST_F(NavigationManagerTest, AddSameUrlPendingItemIfOverrideDesktop) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(
      web::UserAgentType::MOBILE,
      navigation_manager()->GetLastCommittedItem()->GetUserAgentType(nil));
  EXPECT_EQ(web::UserAgentType::MOBILE, navigation_manager()
                                            ->GetLastCommittedItem()
                                            ->GetUserAgentForInheritance());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL succeeds if the user agent override option is MOBILE.
TEST_F(NavigationManagerTest, AddSameUrlPendingItemIfOverrideMobile) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(
      web::UserAgentType::DESKTOP,
      navigation_manager()->GetLastCommittedItem()->GetUserAgentType(nil));
  EXPECT_EQ(web::UserAgentType::DESKTOP, navigation_manager()
                                             ->GetLastCommittedItem()
                                             ->GetUserAgentForInheritance());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that desktop user agent can be enforced to use for next pending item
// when UserAgentOverrideOption is DESKTOP.
TEST_F(NavigationManagerTest, OverrideUserAgentWithDesktop) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  NavigationItem* last_committed_item =
      navigation_manager()->GetLastCommittedItem();
  EXPECT_EQ(UserAgentType::MOBILE, last_committed_item->GetUserAgentType(nil));
  EXPECT_EQ(UserAgentType::MOBILE,
            last_committed_item->GetUserAgentForInheritance());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(UserAgentType::DESKTOP,
            navigation_manager()->GetPendingItem()->GetUserAgentType(nil));
  EXPECT_EQ(
      UserAgentType::DESKTOP,
      navigation_manager()->GetPendingItem()->GetUserAgentForInheritance());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that mobile user agent can be enforced to use for next pending item
// when UserAgentOverrideOption is MOBILE.
TEST_F(NavigationManagerTest, OverrideUserAgentWithMobile) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  NavigationItem* last_committed_item =
      navigation_manager()->GetLastCommittedItem();
  last_committed_item->SetUserAgentType(UserAgentType::DESKTOP);
  EXPECT_EQ(UserAgentType::DESKTOP, last_committed_item->GetUserAgentType(nil));
  EXPECT_EQ(UserAgentType::DESKTOP,
            last_committed_item->GetUserAgentForInheritance());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(UserAgentType::MOBILE,
            navigation_manager()->GetPendingItem()->GetUserAgentType(nil));
  EXPECT_EQ(
      UserAgentType::MOBILE,
      navigation_manager()->GetPendingItem()->GetUserAgentForInheritance());
}

// Tests that the UserAgentType of an INHERIT item is propagated to subsequent
// item when UserAgentOverrideOption is INHERIT
TEST_F(NavigationManagerTest, OverrideUserAgentWithInheritAfterInherit) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::MOBILE, navigation_manager()
                                            ->GetLastCommittedItem()
                                            ->GetUserAgentForInheritance());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.2.com"
                  backListURLs:@[ @"http://www.1.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::MOBILE, navigation_manager()
                                            ->GetLastCommittedItem()
                                            ->GetUserAgentForInheritance());
}

// Tests that the UserAgentType of a MOBILE item is propagated to subsequent
// item when UserAgentOverrideOption is INHERIT
TEST_F(NavigationManagerTest, OverrideUserAgentWithInheritAfterMobile) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(
      web::UserAgentType::MOBILE,
      navigation_manager()->GetLastCommittedItem()->GetUserAgentType(nil));
  EXPECT_EQ(web::UserAgentType::MOBILE, navigation_manager()
                                            ->GetLastCommittedItem()
                                            ->GetUserAgentForInheritance());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.2.com"
                  backListURLs:@[ @"http://www.1.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(
      web::UserAgentType::MOBILE,
      navigation_manager()->GetLastCommittedItem()->GetUserAgentType(nil));
  EXPECT_EQ(web::UserAgentType::MOBILE, navigation_manager()
                                            ->GetLastCommittedItem()
                                            ->GetUserAgentForInheritance());
}

// Tests that the UserAgentType of a DESKTOP item is propagated to subsequent
// item when UserAgentOverrideOption is INHERIT
TEST_F(NavigationManagerTest, OverrideUserAgentWithInheritAfterDesktop) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(
      web::UserAgentType::DESKTOP,
      navigation_manager()->GetLastCommittedItem()->GetUserAgentType(nil));
  EXPECT_EQ(web::UserAgentType::DESKTOP, navigation_manager()
                                             ->GetLastCommittedItem()
                                             ->GetUserAgentForInheritance());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.2.com"
                  backListURLs:@[ @"http://www.1.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(
      web::UserAgentType::DESKTOP,
      navigation_manager()->GetLastCommittedItem()->GetUserAgentType(nil));
  EXPECT_EQ(web::UserAgentType::DESKTOP, navigation_manager()
                                             ->GetLastCommittedItem()
                                             ->GetUserAgentForInheritance());
}

// Tests that the UserAgentType is propagated to subsequent NavigationItems if
// an app specific URL exists in between navigations.
TEST_F(NavigationManagerTest, UserAgentTypePropagationPastNativeItems) {
  // This test manipuates the WKBackForwardListItems in mock_wk_list_ directly
  // because it relies on the associated NavigationItems.
  WKBackForwardListItem* wk_item1 =
      [CRWFakeBackForwardList itemWithURLString:@"http://www.1.com"];

  // GURL::Replacements that will replace a GURL's scheme with the test app
  // specific scheme.
  GURL::Replacements app_specific_scheme_replacement;
  app_specific_scheme_replacement.SetSchemeStr(kTestAppSpecificScheme);

  // Create two non-native navigations that are separated by a native one.
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  mock_wk_list_.currentItem = wk_item1;
  navigation_manager()->CommitPendingItem();

  NavigationItem* item1 = navigation_manager()->GetLastCommittedItem();
  ASSERT_EQ(web::UserAgentType::MOBILE, item1->GetUserAgentType(nil));

  GURL item2_url =
      item1->GetURL().ReplaceComponents(app_specific_scheme_replacement);
  navigation_manager()->AddPendingItem(
      item2_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  WKBackForwardListItem* wk_native_item2 = [CRWFakeBackForwardList
      itemWithURLString:base::SysUTF8ToNSString(item2_url.spec())];
  mock_wk_list_.currentItem = wk_native_item2;
  mock_wk_list_.backList = @[ wk_item1 ];
  navigation_manager()->CommitPendingItem();

  NavigationItem* native_item1 = navigation_manager()->GetLastCommittedItem();
  // Having a non-app-specific URL should not change the fact that the native
  // item should be skipped when determining user agent inheritance.
  native_item1->SetVirtualURL(GURL("http://non-app-specific-url"));
  ASSERT_EQ(web::UserAgentType::NONE, native_item1->GetUserAgentType(nil));
  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  WKBackForwardListItem* wk_item2 =
      [CRWFakeBackForwardList itemWithURLString:@"http://www.2.com"];
  mock_wk_list_.currentItem = wk_item2;
  mock_wk_list_.backList = @[ wk_item1, wk_native_item2 ];
  navigation_manager()->CommitPendingItem();
  NavigationItem* item2 = navigation_manager()->GetLastCommittedItem();

  // Verify that |item1|'s UserAgentType is propagated to |item2|.
  EXPECT_EQ(item1->GetUserAgentType(nil), item2->GetUserAgentType(nil));

  // Update |item2|'s UA type to DESKTOP and add a third non-native navigation,
  // once again separated by a native one.
  item2->SetUserAgentType(web::UserAgentType::DESKTOP);
  ASSERT_EQ(web::UserAgentType::DESKTOP, item2->GetUserAgentType(nil));

  GURL item3_url =
      item2->GetURL().ReplaceComponents(app_specific_scheme_replacement);
  navigation_manager()->AddPendingItem(
      item3_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  WKBackForwardListItem* wk_native_item3 = [CRWFakeBackForwardList
      itemWithURLString:base::SysUTF8ToNSString(item3_url.spec())];
  mock_wk_list_.currentItem = wk_native_item3;
  mock_wk_list_.backList = @[ wk_item1, wk_native_item2, wk_item2 ];
  navigation_manager()->CommitPendingItem();

  NavigationItem* native_item2 = navigation_manager()->GetLastCommittedItem();
  ASSERT_EQ(web::UserAgentType::NONE,
            native_item2->GetUserAgentForInheritance());
  navigation_manager()->AddPendingItem(
      GURL("http://www.3.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  WKBackForwardListItem* wk_item3 =
      [CRWFakeBackForwardList itemWithURLString:@"http://www.3.com"];
  mock_wk_list_.currentItem = wk_item3;
  mock_wk_list_.backList =
      @[ wk_item1, wk_native_item2, wk_item2, wk_native_item3 ];
  navigation_manager()->CommitPendingItem();

  NavigationItem* item3 = navigation_manager()->GetLastCommittedItem();

  // Verify that |item2|'s UserAgentType is propagated to |item3|.
  EXPECT_EQ(item2->GetUserAgentForInheritance(),
            item3->GetUserAgentForInheritance());
}

// Tests that adding transient item for a pending item with mobile user agent
// type results in a transient item with mobile user agent type.
TEST_F(NavigationManagerTest, AddTransientItemForMobilePendingItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetUserAgentType(
      UserAgentType::MOBILE);

  navigation_manager()->AddTransientItem(GURL("http://www.url.com"));
  ASSERT_TRUE(navigation_manager()->GetTransientItem());
  EXPECT_EQ(
      UserAgentType::MOBILE,
      navigation_manager()->GetTransientItem()->GetUserAgentForInheritance());
  EXPECT_EQ(
      UserAgentType::MOBILE,
      navigation_manager()->GetPendingItem()->GetUserAgentForInheritance());
}

// Tests that adding transient item for a pending item with desktop user agent
// type results in a transient item with desktop user agent type.
TEST_F(NavigationManagerTest, AddTransientItemForDesktopPendingItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetUserAgentType(
      UserAgentType::DESKTOP);

  navigation_manager()->AddTransientItem(GURL("http://www.url.com"));
  ASSERT_TRUE(navigation_manager()->GetTransientItem());
  EXPECT_EQ(
      UserAgentType::DESKTOP,
      navigation_manager()->GetTransientItem()->GetUserAgentForInheritance());
  EXPECT_EQ(
      UserAgentType::DESKTOP,
      navigation_manager()->GetPendingItem()->GetUserAgentForInheritance());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL is no-op when there
// are no transient, pending and committed items.
TEST_F(NavigationManagerTest, ReloadEmptyWithNormalType) {
  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(0);
  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);

  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the renderer initiated pending item unchanged when there is one.
TEST_F(NavigationManagerTest, ReloadRendererPendingItemWithNormalType) {
  GURL url_before_reload = GURL("http://www.url.com");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the user initiated pending item unchanged when there is one.
TEST_F(NavigationManagerTest, ReloadUserPendingItemWithNormalType) {
  GURL url_before_reload = GURL("http://www.url.com");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the last committed item unchanged when there is no pending item.
TEST_F(NavigationManagerTest, ReloadLastCommittedItemWithNormalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  GURL url_before_reload = GURL("http://www.url.com/1");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the last committed item unchanged when there is no pending item, but there
// forward items after last committed item.
TEST_F(NavigationManagerTest,
       ReloadLastCommittedItemWithNormalTypeWithForwardItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  GURL url_before_reload = GURL("http://www.url.com/1");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_
        setCurrentURL:@"http://www.url.com/2"
         backListURLs:@[ @"http://www.url.com/0", @"http://www.url.com/1" ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  SimulateGoToIndex(1);
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL is
// no-op when there are no transient, pending and committed items.
TEST_F(NavigationManagerTest, ReloadEmptyWithOriginalType) {
  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(0);
  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);

  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the renderer initiated pending item's url to its original request url
// when there is one.
TEST_F(NavigationManagerTest, ReloadRendererPendingItemWithOriginalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  GURL expected_original_url = GURL("http://www.url.com/original");
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the user initiated pending item's url to its original request url
// when there is one.
TEST_F(NavigationManagerTest, ReloadUserPendingItemWithOriginalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  GURL expected_original_url = GURL("http://www.url.com/original");
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the last committed item's url to its original request url when there
// is no pending item.
TEST_F(NavigationManagerTest, ReloadLastCommittedItemWithOriginalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL expected_original_url = GURL("http://www.url.com/1/original");
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1/original"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the last committed item's url to its original request url when there
// is no pending item, but there are forward items after last committed item.
TEST_F(NavigationManagerTest,
       ReloadLastCommittedItemWithOriginalTypeWithForwardItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL expected_original_url = GURL("http://www.url.com/1/original");
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1/original"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/2"
                  backListURLs:@[
                    @"http://www.url.com/0", @"http://www.url.com/1/original"
                  ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  SimulateGoToIndex(1);
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that ReloadWithUserAgentType triggers new navigation with the expected
// user agent override.
TEST_F(NavigationManagerTest, ReloadWithUserAgentType) {
  GURL url("http://www.1.com");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      NavigationInitiationType::BROWSER_INITIATED,
      NavigationManager::UserAgentOverrideOption::MOBILE);
  GURL virtual_url("http://www.1.com/virtual");
  navigation_manager()->GetPendingItem()->SetVirtualURL(virtual_url);
  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();
  OCMStub([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.1.com"]);

  EXPECT_CALL(navigation_manager_delegate(), RecordPageStateInNavigationItem());
  EXPECT_CALL(navigation_manager_delegate(), ClearTransientContent());
  EXPECT_CALL(navigation_manager_delegate(), ClearDialogs());
  EXPECT_CALL(navigation_manager_delegate(),
              LoadCurrentItem(NavigationInitiationType::BROWSER_INITIATED));

  navigation_manager()->ReloadWithUserAgentType(UserAgentType::DESKTOP);

  NavigationItem* pending_item =
      navigation_manager()->GetPendingItemInCurrentOrRestoredSession();
  EXPECT_EQ(url, pending_item->GetURL());
  EXPECT_EQ(virtual_url, pending_item->GetVirtualURL());
  EXPECT_EQ(UserAgentType::DESKTOP, pending_item->GetUserAgentType(nil));
  EXPECT_EQ(UserAgentType::DESKTOP, pending_item->GetUserAgentForInheritance());
}

// Tests that ReloadWithUserAgentType reloads on the last committed item before
// the redirect items.
TEST_F(NavigationManagerTest, ReloadWithUserAgentTypeOnRedirect) {
  GURL url("http://www.1.com");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      NavigationInitiationType::BROWSER_INITIATED,
      NavigationManager::UserAgentOverrideOption::MOBILE);
  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.redirect.com"), Referrer(),
      ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      NavigationInitiationType::BROWSER_INITIATED,
      NavigationManager::UserAgentOverrideOption::MOBILE);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/redirect"
                  backListURLs:@[ @"http://www.1.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->ReloadWithUserAgentType(UserAgentType::DESKTOP);

  NavigationItem* pending_item =
      navigation_manager()->GetPendingItemInCurrentOrRestoredSession();
  EXPECT_EQ(url, pending_item->GetURL());
  EXPECT_EQ(UserAgentType::DESKTOP, pending_item->GetUserAgentType(nil));
  EXPECT_EQ(UserAgentType::DESKTOP, pending_item->GetUserAgentForInheritance());
}

// Tests that ReloadWithUserAgentType reloads on the last committed item if
// there are no item before a redirect (which happens when opening a new tab on
// a redirect).
TEST_F(NavigationManagerTest, ReloadWithUserAgentTypeOnNewTabRedirect) {
  GURL url("http://www.1.com");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      NavigationInitiationType::BROWSER_INITIATED,
      NavigationManager::UserAgentOverrideOption::MOBILE);
  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->ReloadWithUserAgentType(UserAgentType::DESKTOP);

  NavigationItem* pending_item =
      navigation_manager()->GetPendingItemInCurrentOrRestoredSession();
  EXPECT_EQ(url, pending_item->GetURL());
  EXPECT_EQ(UserAgentType::DESKTOP, pending_item->GetUserAgentType(nil));
  EXPECT_EQ(UserAgentType::DESKTOP, pending_item->GetUserAgentForInheritance());
}

// Tests that ReloadWithUserAgentType does not expose internal URLs.
TEST_F(NavigationManagerTest, ReloadWithUserAgentTypeOnIntenalUrl) {
  delegate_.SetWebState(&web_state_);
  web_state_.SetLoading(true);

  GURL url = wk_navigation_util::CreateRedirectUrl(GURL("http://www.1.com"));
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      NavigationInitiationType::BROWSER_INITIATED,
      NavigationManager::UserAgentOverrideOption::MOBILE);
  GURL virtual_url("http://www.1.com/virtual");
  navigation_manager()
      ->GetPendingItemInCurrentOrRestoredSession()
      ->SetVirtualURL(virtual_url);
  [mock_wk_list_ setCurrentURL:base::SysUTF8ToNSString(url.spec())];
  navigation_manager()->OnNavigationStarted(GURL("http://www.1.com/virtual"));

  navigation_manager()->ReloadWithUserAgentType(UserAgentType::DESKTOP);

  NavigationItem* pending_item =
      navigation_manager()->GetPendingItemInCurrentOrRestoredSession();
  EXPECT_EQ(url, pending_item->GetURL());
  EXPECT_EQ(virtual_url, pending_item->GetVirtualURL());
  EXPECT_EQ(UserAgentType::DESKTOP, pending_item->GetUserAgentType(nil));
  EXPECT_EQ(UserAgentType::DESKTOP, pending_item->GetUserAgentForInheritance());
}

// Tests that app-specific URLs are not rewritten for renderer-initiated loads
// or reloads unless requested by a page with app-specific url.
TEST_F(NavigationManagerTest, RewritingAppSpecificUrls) {
  // URL should not be rewritten as there is no committed URL.
  GURL url1(url::SchemeHostPort(kSchemeToRewrite, "test", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url1, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(url1, navigation_manager()->GetPendingItem()->GetURL());

  // URL should not be rewritten because last committed URL is not app-specific.
  [mock_wk_list_ setCurrentURL:base::SysUTF8ToNSString(url1.spec())];
  navigation_manager()->CommitPendingItem();

  GURL url2(url::SchemeHostPort(kSchemeToRewrite, "test2", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url2, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(url2, navigation_manager()->GetPendingItem()->GetURL());

  // URL should not be rewritten for user initiated reload navigations.
  GURL url_reload(
      url::SchemeHostPort(kSchemeToRewrite, "test-reload", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url_reload, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(url_reload, navigation_manager()->GetPendingItem()->GetURL());

  // URL should be rewritten for user initiated navigations.
  GURL url3(url::SchemeHostPort(kSchemeToRewrite, "test3", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url3, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL rewritten_url3(
      url::SchemeHostPort(kTestWebUIScheme, "test3", 0).Serialize());
  EXPECT_EQ(rewritten_url3, navigation_manager()->GetPendingItem()->GetURL());

  // URL should be rewritten because last committed URL is app-specific.
  [mock_wk_list_ setCurrentURL:base::SysUTF8ToNSString(rewritten_url3.spec())
                  backListURLs:@[ base::SysUTF8ToNSString(url1.spec()) ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  GURL url4(url::SchemeHostPort(kSchemeToRewrite, "test4", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url4, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL rewritten_url4(
      url::SchemeHostPort(kTestWebUIScheme, "test4", 0).Serialize());
  EXPECT_EQ(rewritten_url4, navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that transient URLRewriters are applied for pending items.
TEST_F(NavigationManagerTest, ApplyTransientRewriters) {
  navigation_manager()->AddTransientURLRewriter(&AppendingUrlRewriter);
  navigation_manager()->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  NavigationItem* pending_item = navigation_manager()->GetPendingItem();
  EXPECT_EQ(kRewrittenQueryParam, pending_item->GetURL().query());

  // Now that the transient rewriters are consumed, the next URL should not be
  // changed.
  GURL url("http://www.1.com");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_EQ(url, navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that GetIndexOfItem() returns the correct values.
TEST_F(NavigationManagerTest, GetIndexOfItem) {
  // This test manipuates the WKBackForwardListItems in mock_wk_list_ directly
  // to retain the NavigationItem association.
  WKBackForwardListItem* wk_item0 =
      [CRWFakeBackForwardList itemWithURLString:@"http://www.url.com/0"];
  WKBackForwardListItem* wk_item1 =
      [CRWFakeBackForwardList itemWithURLString:@"http://www.url.com/1"];

  // Create two items and add them to the NavigationManagerImpl.
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  mock_wk_list_.currentItem = wk_item0;
  navigation_manager()->CommitPendingItem();

  NavigationItem* item0 = navigation_manager()->GetLastCommittedItem();
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  mock_wk_list_.currentItem = wk_item1;
  mock_wk_list_.backList = @[ wk_item0 ];
  navigation_manager()->CommitPendingItem();

  NavigationItem* item1 = navigation_manager()->GetLastCommittedItem();
  // Create an item that does not exist in the NavigationManagerImpl.
  std::unique_ptr<NavigationItem> item_not_found = NavigationItem::Create();
  // Verify GetIndexOfItem() results.
  EXPECT_EQ(0, navigation_manager()->GetIndexOfItem(item0));
  EXPECT_EQ(1, navigation_manager()->GetIndexOfItem(item1));
  EXPECT_EQ(-1, navigation_manager()->GetIndexOfItem(item_not_found.get()));
}

// Tests that GetBackwardItems() and GetForwardItems() return expected entries
// when current item is in the middle of the navigation history.
TEST_F(NavigationManagerTest, TestBackwardForwardItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_
        setCurrentURL:@"http://www.url.com/2"
         backListURLs:@[ @"http://www.url.com/0", @"http://www.url.com/1" ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(2, navigation_manager()->GetLastCommittedItemIndex());
  NavigationItemList back_items = navigation_manager()->GetBackwardItems();
  EXPECT_EQ(2U, back_items.size());
  EXPECT_EQ("http://www.url.com/1", back_items[0]->GetURL().spec());
  EXPECT_EQ("http://www.url.com/0", back_items[1]->GetURL().spec());
  EXPECT_TRUE(navigation_manager()->GetForwardItems().empty());

  SimulateGoToIndex(1);
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  back_items = navigation_manager()->GetBackwardItems();
  EXPECT_EQ(1U, back_items.size());
  EXPECT_EQ("http://www.url.com/0", back_items[0]->GetURL().spec());
  NavigationItemList forward_items = navigation_manager()->GetForwardItems();
  EXPECT_EQ(1U, forward_items.size());
  EXPECT_EQ("http://www.url.com/2", forward_items[0]->GetURL().spec());
}

// Tests that Restore() creates the correct navigation state.
TEST_F(NavigationManagerTest, Restore) {
  ItemInfoToBeRestored restore_information[3];
  restore_information[0] = {GURL("http://www.url.com/0"),
                            GURL("http://virtual/0"), UserAgentType::MOBILE,
                            PageDisplayState()};
  restore_information[1] = {GURL("http://www.url.com/1"), GURL(),
                            UserAgentType::AUTOMATIC, PageDisplayState()};
  restore_information[2] = {GURL("http://www.url.com/2"),
                            GURL("http://virtual/2"), UserAgentType::DESKTOP,
                            PageDisplayState()};

  std::vector<std::unique_ptr<NavigationItem>> items;
  for (size_t index = 0; index < base::size(restore_information); ++index) {
    items.push_back(NavigationItem::Create());
    items.back()->SetURL(restore_information[index].url);
    items.back()->SetVirtualURL(restore_information[index].virtual_url);
    items.back()->SetUserAgentType(restore_information[index].user_agent);
    items.back()->SetPageDisplayState(restore_information[index].display_state);
  }

  // Call Restore() and check that the NavigationItems are in the correct order
  // and that the last committed index is correct too.
  ASSERT_FALSE(navigation_manager()->IsRestoreSessionInProgress());
  navigation_manager()->Restore(1, std::move(items));
  __block bool restore_done = false;
  navigation_manager()->AddRestoreCompletionCallback(base::BindOnce(^{
    navigation_manager()->CommitPendingItem();
    restore_done = true;
  }));

  // Session restore is asynchronous for WKBasedNavigationManager.
  ASSERT_TRUE(navigation_manager()->IsRestoreSessionInProgress());
  ASSERT_FALSE(restore_done);

  // Verify that restore session URL is pending.
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  NavigationItem* pending_item =
      navigation_manager()->GetPendingItemInCurrentOrRestoredSession();
  ASSERT_TRUE(pending_item);
  GURL pending_url = pending_item->GetURL();
  EXPECT_TRUE(pending_url.SchemeIsFile());
  EXPECT_EQ("restore_session.html", pending_url.ExtractFileName());
  EXPECT_EQ("http://virtual/0", pending_item->GetVirtualURL());
  navigation_manager()->OnNavigationStarted(pending_url);

  // Simulate the end effect of loading the restore session URL in web view.
  pending_item->SetURL(restore_information[1].url);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:@[ @"http://www.url.com/2" ]];
  navigation_manager()->OnNavigationStarted(GURL("http://www.url.com/2"));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return restore_done;
  }));

  EXPECT_FALSE(navigation_manager()->IsRestoreSessionInProgress());
  ASSERT_EQ(3, navigation_manager()->GetItemCount());
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_EQ(restore_information[1].url,
            navigation_manager()->GetLastCommittedItem()->GetURL());

  for (size_t i = 0; i < base::size(restore_information); ++i) {
    NavigationItem* navigation_item = navigation_manager()->GetItemAtIndex(i);
    EXPECT_EQ(restore_information[i].url, navigation_item->GetURL());
    if (@available(iOS 13, *)) {
      if (!restore_information[i].virtual_url.is_empty()) {
        EXPECT_EQ(restore_information[i].virtual_url,
                  navigation_item->GetVirtualURL());
      }
      EXPECT_EQ(restore_information[i].user_agent,
                navigation_item->GetUserAgentForInheritance());
    }
    EXPECT_EQ(restore_information[i].display_state,
              navigation_item->GetPageDisplayState());
  }

  histogram_tester_.ExpectTotalCount(kRestoreNavigationItemCount, 1);
  histogram_tester_.ExpectBucketCount(kRestoreNavigationItemCount, 3, 1);
}

// Tests that pending item is not considered part of session history so that
// GetBackwardItems returns the second last committed item even if there is a
// pendign item.
TEST_F(NavigationManagerTest, NewPendingItemIsHiddenFromHistory) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_TRUE(navigation_manager()->GetPendingItem());

  NavigationItemList back_items = navigation_manager()->GetBackwardItems();
  EXPECT_EQ(1U, back_items.size());
  EXPECT_EQ("http://www.url.com/0", back_items[0]->GetURL().spec());
}

// Tests that all committed items are considered history if there is a transient
// item. This can happen if an interstitial was loaded for SSL error.
// See crbug.com/691311.
TEST_F(NavigationManagerTest,
       BackwardItemsShouldContainAllCommittedIfCurrentIsTransient) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/1"));

  EXPECT_EQ(0, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_TRUE(navigation_manager()->GetTransientItem());

  NavigationItemList back_items = navigation_manager()->GetBackwardItems();
  EXPECT_EQ(1U, back_items.size());
  EXPECT_EQ("http://www.url.com/0", back_items[0]->GetURL().spec());
}

TEST_F(NavigationManagerTest, BackwardItemsShouldBeEmptyIfFirstIsTransient) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/0"));

  EXPECT_EQ(-1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_TRUE(navigation_manager()->GetTransientItem());

  NavigationItemList back_items = navigation_manager()->GetBackwardItems();
  EXPECT_TRUE(back_items.empty());
}

TEST_F(NavigationManagerTest, VisibleItemIsTransientItemIfPresent) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/1"));

  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  EXPECT_EQ("http://www.url.com/1",
            navigation_manager()->GetVisibleItem()->GetURL().spec());

  // Visible item is still transient item even if there is a committed item.
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/3"));

  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  EXPECT_EQ("http://www.url.com/3",
            navigation_manager()->GetVisibleItem()->GetURL().spec());
}

TEST_F(NavigationManagerTest, PendingItemIsVisibleIfNewAndUserInitiated) {
  delegate_.SetWebState(&web_state_);
  web_state_.SetLoading(true);

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  EXPECT_EQ("http://www.url.com/0",
            navigation_manager()->GetVisibleItem()->GetURL().spec());

  // Visible item is still the user initiated pending item even if there is a
  // committed item.
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  EXPECT_EQ("http://www.url.com/1",
            navigation_manager()->GetVisibleItem()->GetURL().spec());
}

TEST_F(NavigationManagerTest, PendingItemIsNotVisibleIfNotUserInitiated) {
  delegate_.SetWebState(&web_state_);
  web_state_.SetLoading(true);

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_EQ(nullptr, navigation_manager()->GetVisibleItem());
}

TEST_F(NavigationManagerTest, PendingItemIsNotVisibleIfNotNewNavigation) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  // Move pending item back to index 0.
  OCMStub([mock_web_view_ URL])
      .andReturn([NSURL URLWithString:@"http://www.url.com/0"]);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                  backListURLs:nil
               forwardListURLs:@[ @"http://www.url.com/1" ]];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(),
      ui::PAGE_TRANSITION_FORWARD_BACK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_EQ(0, navigation_manager()->GetPendingItemIndex());

  delegate_.SetWebState(&web_state_);
  web_state_.SetLoading(true);
  OCMStub([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.url.com/0"]);
  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  OCMStub([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.url.com/0"]);
  EXPECT_EQ("http://www.url.com/1",
            navigation_manager()->GetVisibleItem()->GetURL().spec());
}

TEST_F(NavigationManagerTest, VisibleItemDefaultsToLastCommittedItem) {
  delegate_.SetWebState(&web_state_);
  web_state_.SetLoading(true);

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  OCMStub([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.url.com/0"]);
  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  OCMStub([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.url.com/0"]);
  EXPECT_EQ("http://www.url.com/0",
            navigation_manager()->GetVisibleItem()->GetURL().spec());
}

// Tests that |extra_headers| and |post_data| from WebLoadParams are added to
// the new navigation item if they are present.
TEST_F(NavigationManagerTest, LoadURLWithParamsWithExtraHeadersAndPostData) {
  NavigationManager::WebLoadParams params(GURL("http://www.url.com/0"));
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  params.extra_headers = @{@"Content-Type" : @"text/plain"};
  params.post_data = [NSData data];

  EXPECT_CALL(navigation_manager_delegate(), RecordPageStateInNavigationItem())
      .Times(1);
  EXPECT_CALL(navigation_manager_delegate(), ClearTransientContent()).Times(1);
  EXPECT_CALL(navigation_manager_delegate(), ClearDialogs()).Times(1);
  EXPECT_CALL(navigation_manager_delegate(),
              LoadCurrentItem(NavigationInitiationType::BROWSER_INITIATED))
      .Times(1);

  navigation_manager()->LoadURLWithParams(params);

  NavigationItem* pending_item = navigation_manager()->GetPendingItem();
  ASSERT_TRUE(pending_item);
  EXPECT_EQ("http://www.url.com/0", pending_item->GetURL().spec());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(pending_item->GetTransitionType(),
                                           ui::PAGE_TRANSITION_TYPED));
  EXPECT_NSEQ(pending_item->GetHttpRequestHeaders(),
              @{@"Content-Type" : @"text/plain"});
  EXPECT_TRUE(pending_item->HasPostData());
}

// Tests that LoadURLWithParams() calls RecordPageStateInNavigationItem() on the
// navigation manager deleget before navigating to the new URL.
TEST_F(NavigationManagerTest, LoadURLWithParamsSavesStateOnCurrentItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  NavigationManager::WebLoadParams params(GURL("http://www.url.com/1"));
  params.transition_type = ui::PAGE_TRANSITION_TYPED;

  EXPECT_CALL(navigation_manager_delegate(), RecordPageStateInNavigationItem())
      .Times(1);
  EXPECT_CALL(navigation_manager_delegate(), ClearTransientContent()).Times(1);
  EXPECT_CALL(navigation_manager_delegate(), ClearDialogs()).Times(1);
  EXPECT_CALL(navigation_manager_delegate(),
              LoadCurrentItem(NavigationInitiationType::BROWSER_INITIATED))
      .Times(1);

  navigation_manager()->LoadURLWithParams(params);

  NavigationItem* last_committed_item =
      navigation_manager()->GetLastCommittedItem();
  ASSERT_TRUE(last_committed_item);
  EXPECT_EQ("http://www.url.com/0", last_committed_item->GetURL().spec());
  NavigationItem* pending_item = navigation_manager()->GetPendingItem();
  ASSERT_TRUE(pending_item);
  EXPECT_EQ("http://www.url.com/1", pending_item->GetURL().spec());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(pending_item->GetTransitionType(),
                                           ui::PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(pending_item->HasPostData());
}

TEST_F(NavigationManagerTest, UpdatePendingItemWithoutPendingItem) {
  navigation_manager()->UpdatePendingItemUrl(GURL("http://another.url.com"));
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
}

TEST_F(NavigationManagerTest, UpdatePendingItemWithPendingItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->UpdatePendingItemUrl(GURL("http://another.url.com"));

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ("http://another.url.com/",
            navigation_manager()->GetPendingItem()->GetURL().spec());
}

TEST_F(NavigationManagerTest,
       UpdatePendingItemWithPendingItemAlreadyCommitted) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();
  navigation_manager()->UpdatePendingItemUrl(GURL("http://another.url.com"));

  ASSERT_EQ(1, navigation_manager()->GetItemCount());
  EXPECT_EQ("http://www.url.com/",
            navigation_manager()->GetItemAtIndex(0)->GetURL().spec());
}

// Tests that LoadCurrentItem() is exercised when going to a different page.
TEST_F(NavigationManagerTest, GoToIndexDifferentDocument) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_FALSE(navigation_manager()->GetItemAtIndex(0)->GetTransitionType() &
               ui::PAGE_TRANSITION_FORWARD_BACK);

  EXPECT_CALL(navigation_manager_delegate(), RecordPageStateInNavigationItem());
  EXPECT_CALL(navigation_manager_delegate(), ClearTransientContent());
  EXPECT_CALL(navigation_manager_delegate(), ClearDialogs());

  navigation_manager()->GoToIndex(0);
  EXPECT_TRUE(navigation_manager()->GetItemAtIndex(0)->GetTransitionType() &
              ui::PAGE_TRANSITION_FORWARD_BACK);
}

// Tests that LoadCurrentItem() is not exercised for same-document navigation.
TEST_F(NavigationManagerTest, GoToIndexSameDocument) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0#hash"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  static_cast<NavigationItemImpl*>(navigation_manager()->GetPendingItem())
      ->SetIsCreatedFromHashChange(true);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0#hash"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_FALSE(navigation_manager()->GetItemAtIndex(0)->GetTransitionType() &
               ui::PAGE_TRANSITION_FORWARD_BACK);

  EXPECT_CALL(navigation_manager_delegate(), RecordPageStateInNavigationItem());
  EXPECT_CALL(navigation_manager_delegate(), ClearTransientContent());
  EXPECT_CALL(navigation_manager_delegate(), ClearDialogs());

  navigation_manager()->GoToIndex(0);
  EXPECT_TRUE(navigation_manager()->GetItemAtIndex(0)->GetTransitionType() &
              ui::PAGE_TRANSITION_FORWARD_BACK);
}

// Tests that NavigationManagerImpl::CommitPendingItem() is no-op when called
// with null.
TEST_F(NavigationManagerTest, CommitNilPendingItem) {
  ASSERT_EQ(0, navigation_manager()->GetItemCount());
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:nil
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem(nullptr);

  EXPECT_EQ(1, navigation_manager()->GetItemCount());
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ("http://www.url.com/0",
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that NavigationManagerImpl::CommitPendingItem() for an invalid URL
// doesn't crash.
TEST_F(NavigationManagerTest, CommitEmptyPendingItem) {
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:nil
               forwardListURLs:nil];

  // Call CommitPendingItem() with a valid pending item.
  auto item = std::make_unique<web::NavigationItemImpl>();
  item->SetURL(GURL::EmptyGURL());
  navigation_manager()->CommitPendingItem(std::move(item));
}

// Tests NavigationManagerImpl::CommitPendingItem() with a valid pending item.
TEST_F(NavigationManagerTest, CommitNonNilPendingItem) {
  // Create navigation manager with a single forward item and no back items.
  [mock_wk_list_ setCurrentURL:@"http://www.url.test"
                  backListURLs:@[
                    @"www.url.test/0",
                  ]
               forwardListURLs:nil];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.test/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->CommitPendingItem();
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.test/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->CommitPendingItem();
  SimulateGoToIndex(0);
  mock_wk_list_.backList = @[ mock_wk_list_.currentItem ];
  mock_wk_list_.currentItem =
      [CRWFakeBackForwardList itemWithURLString:@"http://www.url.com/new"];
  mock_wk_list_.forwardList = nil;
  ASSERT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_EQ(2, navigation_manager()->GetItemCount());

  // Emulate 2 simultanious navigations to verify that pending item index does
  // not prevent passed item commit.
  navigation_manager()->SetPendingItemIndex(0);

  // Call CommitPendingItem() with a valid pending item.
  auto item = std::make_unique<web::NavigationItemImpl>();
  item->SetURL(GURL("http://www.url.com/new"));
  item->SetNavigationInitiationType(
      web::NavigationInitiationType::BROWSER_INITIATED);
  navigation_manager()->CommitPendingItem(std::move(item));

  // Verify navigation manager and navigation item states.
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_FALSE(
      navigation_manager()->GetLastCommittedItem()->GetTimestamp().is_null());
  EXPECT_EQ(web::NavigationInitiationType::NONE,
            navigation_manager()
                ->GetLastCommittedItemImpl()
                ->NavigationInitiationType());
  ASSERT_EQ(2, navigation_manager()->GetItemCount());
  EXPECT_EQ(navigation_manager()->GetLastCommittedItem(),
            navigation_manager()->GetItemAtIndex(1));
}

TEST_F(NavigationManagerTest, LoadIfNecessary) {
  EXPECT_CALL(navigation_manager_delegate(), LoadIfNecessary()).Times(1);
  navigation_manager()->LoadIfNecessary();
}

// Tests that GetCurrentItemImpl() returns the transient item, pending item or
// last committed item in that precedence order.
TEST_F(NavigationManagerTest, GetCurrentItemImpl) {
  ASSERT_EQ(nullptr, navigation_manager()->GetCurrentItemImpl());

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();
  NavigationItem* last_committed_item =
      navigation_manager()->GetLastCommittedItem();
  ASSERT_NE(last_committed_item, nullptr);
  EXPECT_EQ(last_committed_item, navigation_manager()->GetCurrentItemImpl());

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  NavigationItem* pending_item = navigation_manager()->GetPendingItem();
  ASSERT_NE(pending_item, nullptr);
  EXPECT_EQ(pending_item, navigation_manager()->GetCurrentItemImpl());

  navigation_manager()->AddTransientItem(GURL("http://www.url.com/2"));
  NavigationItem* transient_item = navigation_manager()->GetTransientItem();
  ASSERT_NE(transient_item, nullptr);
  EXPECT_EQ(transient_item, navigation_manager()->GetCurrentItemImpl());
}

TEST_F(NavigationManagerTest, UpdateCurrentItemForReplaceState) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"),
      Referrer(GURL("http://referrer.com"), ReferrerPolicyDefault),
      ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  // Tests that pending item can be replaced.
  GURL replace_page_url("http://www.url.com/replace");
  NSString* state_object = @"{'foo': 1}";

  // Replace current item and check history size and fields of the modified
  // item.
  navigation_manager()->UpdateCurrentItemForReplaceState(replace_page_url,
                                                         state_object);

  EXPECT_EQ(0, navigation_manager()->GetItemCount());
  auto* pending_item =
      static_cast<NavigationItemImpl*>(navigation_manager()->GetPendingItem());
  EXPECT_EQ(replace_page_url, pending_item->GetURL());
  EXPECT_FALSE(pending_item->IsCreatedFromPushState());
  EXPECT_NSEQ(state_object, pending_item->GetSerializedStateObject());
  EXPECT_EQ(GURL("http://referrer.com"), pending_item->GetReferrer().url);

  // Commit pending item and tests that replace updates the committed item.
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  // Replace current item again and check history size and fields.
  GURL replace_page_url2("http://www.url.com/replace2");
  navigation_manager()->UpdateCurrentItemForReplaceState(replace_page_url2,
                                                         nil);

  EXPECT_EQ(1, navigation_manager()->GetItemCount());
  auto* last_committed_item = static_cast<NavigationItemImpl*>(
      navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(replace_page_url2, last_committed_item->GetURL());
  EXPECT_FALSE(last_committed_item->IsCreatedFromPushState());
  EXPECT_NSEQ(nil, last_committed_item->GetSerializedStateObject());
  EXPECT_EQ(GURL("http://referrer.com"),
            last_committed_item->GetReferrer().url);
}

// Tests SetPendingItem() and ReleasePendingItem() methods.
TEST_F(NavigationManagerTest, TransferPendingItem) {
  auto item = std::make_unique<web::NavigationItemImpl>();
  web::NavigationItemImpl* item_ptr = item.get();

  navigation_manager()->SetPendingItem(std::move(item));
  EXPECT_EQ(item_ptr, navigation_manager()->GetPendingItem());

  auto extracted_item = navigation_manager()->ReleasePendingItem();
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(item_ptr, extracted_item.get());
}

class NavigationManagerImplUtilTest : public PlatformTest {
 protected:
  web::TestNavigationManager nav_manager_;
};

// Tests that empty navigation manager returns nullptr.
TEST_F(NavigationManagerImplUtilTest, TestLastNonRedirectedItemEmpty) {
  EXPECT_FALSE(
      NavigationManagerImpl::GetLastCommittedNonRedirectedItem(nullptr));
  EXPECT_FALSE(
      NavigationManagerImpl::GetLastCommittedNonRedirectedItem(&nav_manager_));
}

// Tests that typed in URL works correctly.
TEST_F(NavigationManagerImplUtilTest, TestLastNonRedirectedItemTypedUrl) {
  nav_manager_.AddItem(GURL("http://foo.com/page0"), ui::PAGE_TRANSITION_TYPED);
  NavigationItem* item =
      NavigationManagerImpl::GetLastCommittedNonRedirectedItem(&nav_manager_);
  ASSERT_TRUE(item);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      item->GetTransitionType(), ui::PAGE_TRANSITION_TYPED));
}

// Tests that link click works correctly.
TEST_F(NavigationManagerImplUtilTest, TestLastNonRedirectedItemLinkClicked) {
  nav_manager_.AddItem(GURL("http://foo.com/page0"), ui::PAGE_TRANSITION_LINK);
  NavigationItem* item =
      NavigationManagerImpl::GetLastCommittedNonRedirectedItem(&nav_manager_);
  ASSERT_TRUE(item);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      item->GetTransitionType(), ui::PAGE_TRANSITION_LINK));
}

// Tests that redirect items are skipped.
TEST_F(NavigationManagerImplUtilTest,
       TestLastNonRedirectedItemLinkMultiRedirects) {
  nav_manager_.AddItem(GURL("http://foo.com/page0"), ui::PAGE_TRANSITION_LINK);
  nav_manager_.AddItem(GURL("http://bar.com/redir1"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  nav_manager_.AddItem(GURL("http://bar.com/redir2"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  NavigationItem* item =
      NavigationManagerImpl::GetLastCommittedNonRedirectedItem(&nav_manager_);
  ASSERT_TRUE(item);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      item->GetTransitionType(), ui::PAGE_TRANSITION_LINK));
}

// Tests that when all items are redirects, nullptr is returned.
TEST_F(NavigationManagerImplUtilTest, TestLastNonRedirectedItemAllRedirects) {
  nav_manager_.AddItem(GURL("http://bar.com/redir0"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  nav_manager_.AddItem(GURL("http://bar.com/redir1"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  nav_manager_.AddItem(GURL("http://bar.com/redir2"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  NavigationItem* item =
      NavigationManagerImpl::GetLastCommittedNonRedirectedItem(&nav_manager_);
  EXPECT_FALSE(item);
}

// Tests that earlier redirects are not found.
TEST_F(NavigationManagerImplUtilTest, TestLastNonRedirectedItemNotEarliest) {
  nav_manager_.AddItem(GURL("http://foo.com/bookmark"),
                       ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  nav_manager_.AddItem(GURL("http://foo.com/page0"), ui::PAGE_TRANSITION_TYPED);
  nav_manager_.AddItem(GURL("http://bar.com/redir1"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  nav_manager_.AddItem(GURL("http://bar.com/redir2"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  NavigationItem* item =
      NavigationManagerImpl::GetLastCommittedNonRedirectedItem(&nav_manager_);
  ASSERT_TRUE(item);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      item->GetTransitionType(), ui::PAGE_TRANSITION_TYPED));
}

}  // namespace web
