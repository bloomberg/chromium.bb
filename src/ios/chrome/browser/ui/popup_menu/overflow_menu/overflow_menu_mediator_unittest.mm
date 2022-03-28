// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_mediator.h"

#include "base/files/scoped_temp_dir.h"
#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/default_clock.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_overlay.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_presentation_context.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/policy/enterprise_policy_test_helper.h"
#include "ios/chrome/browser/policy/policy_features.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web/font_size/font_size_tab_helper.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "testing/platform_test.h"
#include "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkModel;
using java_script_dialog_overlays::JavaScriptDialogRequest;

namespace {
const int kNumberOfWebStates = 3;
}  // namespace

class OverflowMenuMediatorTest : public PlatformTest {
 public:
  OverflowMenuMediatorTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(ios::BookmarkModelFactory::GetInstance(),
                              ios::BookmarkModelFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromePasswordStoreFactory::GetInstance(),
        base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                            web::BrowserState,
                            password_manager::MockPasswordStoreInterface>));
    browser_state_ = builder.Build();

    // Set up the TestBrowser.
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    // Set up the WebStateList.
    auto navigation_manager = std::make_unique<ToolbarTestNavigationManager>();

    navigation_item_ = web::NavigationItem::Create();
    navigation_item_->SetURL(GURL("http://chromium.org"));
    navigation_manager->SetVisibleItem(navigation_item_.get());

    std::unique_ptr<web::FakeWebState> test_web_state =
        std::make_unique<web::FakeWebState>();
    test_web_state->SetNavigationManager(std::move(navigation_manager));
    test_web_state->SetLoading(true);
    web_state_ = test_web_state.get();

    browser_->GetWebStateList()->InsertWebState(
        0, std::move(test_web_state), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener());
    for (int i = 1; i < kNumberOfWebStates; i++) {
      InsertNewWebState(i);
    }

    // Set up the OverlayPresenter.
    OverlayPresenter::FromBrowser(browser_.get(),
                                  OverlayModality::kWebContentArea)
        ->SetPresentationContext(&presentation_context_);

    baseViewController_ = [[UIViewController alloc] init];
  }

  void TearDown() override {
    // Explicitly disconnect the mediator so there won't be any WebStateList
    // observers when browser_ gets destroyed.
    [mediator_ disconnect];
    browser_.reset();

    PlatformTest::TearDown();
  }

 protected:
  OverflowMenuMediator* CreateMediator(BOOL is_incognito) {
    mediator_ = [[OverflowMenuMediator alloc] init];
    mediator_.isIncognito = is_incognito;
    mediator_.baseViewController = baseViewController_;
    return mediator_;
  }

  OverflowMenuMediator* CreateMediatorWithBrowserPolicyConnector(
      BOOL is_incognito,
      BrowserPolicyConnectorIOS* browser_policy_connector) {
    mediator_ = [[OverflowMenuMediator alloc] init];
    mediator_.isIncognito = is_incognito;
    mediator_.browserPolicyConnector = browser_policy_connector;
    mediator_.baseViewController = baseViewController_;
    return mediator_;
  }

  void CreatePrefs() {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterBooleanPref(
        bookmarks::prefs::kEditBookmarksEnabled,
        /*default_value=*/true);
  }

  void SetUpBookmarks() {
    bookmark_model_ =
        ios::BookmarkModelFactory::GetForBrowserState(browser_state_.get());
    DCHECK(bookmark_model_);
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
    mediator_.bookmarkModel = bookmark_model_;
  }

  void InsertNewWebState(int index) {
    auto web_state = std::make_unique<web::FakeWebState>();
    GURL url("http://test/" + base::NumberToString(index));
    web_state->SetCurrentURL(url);
    browser_->GetWebStateList()->InsertWebState(
        index, std::move(web_state), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener());
  }

  void SetUpActiveWebState() {
    // OverflowMenuMediator expects an language::IOSLanguageDetectionTabHelper
    // for the currently active WebState.
    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        browser_->GetWebStateList()->GetWebStateAt(0),
        /*url_language_histogram=*/nullptr);

    browser_->GetWebStateList()->ActivateWebStateAt(0);
  }

  // Checks that the overflowMenuModel is receiving a number of items
  // corresponding to |number_items|.
  void CheckMediatorSetItems(NSUInteger destination_items,
                             NSArray<NSNumber*>* action_items) {
    SetUpActiveWebState();
    mediator_.webStateList = browser_->GetWebStateList();
    OverflowMenuModel* model = mediator_.overflowMenuModel;

    EXPECT_EQ(destination_items, model.destinations.count);
    EXPECT_EQ(action_items.count, model.actionGroups.count);

    for (NSUInteger index = 0; index < action_items.count; index++) {
      NSNumber* expected_count = action_items[index];
      EXPECT_EQ(expected_count.unsignedIntValue,
                model.actionGroups[index].actions.count);
    }
  }

  bool HasItem(NSString* accessibility_identifier, BOOL enabled) {
    for (OverflowMenuDestination* destination in mediator_.overflowMenuModel
             .destinations) {
      if (destination.accessibilityIdentifier == accessibility_identifier)
        return YES;
    }
    for (OverflowMenuActionGroup* group in mediator_.overflowMenuModel
             .actionGroups) {
      for (OverflowMenuAction* action in group.actions) {
        if (action.accessibilityIdentifier == accessibility_identifier)
          return action.enabled == enabled;
      }
    }
    return NO;
  }

  bool HasEnterpriseInfoItem() {
    for (OverflowMenuActionGroup* group in mediator_.overflowMenuModel
             .actionGroups) {
      if (group.footer.accessibilityIdentifier == kTextMenuEnterpriseInfo)
        return YES;
    }
    return NO;
  }

  web::WebTaskEnvironment task_env_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;

  FakeOverlayPresentationContext presentation_context_;
  OverflowMenuMediator* mediator_;
  BookmarkModel* bookmark_model_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  web::FakeWebState* web_state_;
  std::unique_ptr<web::NavigationItem> navigation_item_;
  UIViewController* baseViewController_;
};

// Tests that the feature engagement tracker get notified when the mediator is
// disconnected and the tracker wants the notification badge displayed.
TEST_F(OverflowMenuMediatorTest, TestFeatureEngagementDisconnect) {
  CreateMediator(/*is_incognito=*/NO);
  feature_engagement::test::MockTracker tracker;
  EXPECT_CALL(tracker, ShouldTriggerHelpUI(testing::_))
      .WillRepeatedly(testing::Return(true));
  mediator_.engagementTracker = &tracker;

  // Force model creation.
  [mediator_ overflowMenuModel];

  // There may be one or more Tools Menu items that use engagement trackers.
  EXPECT_CALL(tracker, Dismissed(testing::_)).Times(testing::AtLeast(1));
  [mediator_ disconnect];
}

// Tests that the mediator is returning the right number of items and sections
// for the Tools Menu type.
TEST_F(OverflowMenuMediatorTest, TestMenuItemsCount) {
  CreateMediator(/*is_incognito=*/NO);
  NSUInteger number_of_action_items = 5;
  if (ios::provider::IsTextZoomEnabled()) {
    number_of_action_items++;
  }

  // New Tab, New Incognito Tab.
  NSUInteger number_of_tab_actions = 2;
  if (IsSplitToolbarMode(mediator_.baseViewController)) {
    // Stop/Reload only shows in split toolbar mode.
    number_of_tab_actions++;
  }
  if (base::ios::IsMultipleScenesSupported()) {
    // New Window option is added in this case.
    number_of_tab_actions++;
  }

  NSUInteger number_of_help_items = 1;

  if (ios::GetChromeBrowserProvider()
          .GetUserFeedbackProvider()
          ->IsUserFeedbackEnabled()) {
    number_of_help_items++;
  }

  // Checks that Tools Menu has the right number of items in each section.
  CheckMediatorSetItems(8, @[
    @(number_of_tab_actions),
    // Other actions, depending on configuration.
    @(number_of_action_items),
    // Feedback/help actions.
    @(number_of_help_items),
  ]);
}

// Tests that the items returned by the mediator are correctly enabled on a
// WebPage.
TEST_F(OverflowMenuMediatorTest, TestItemsStatusOnWebPage) {
  CreateMediator(/*is_incognito=*/NO);
  SetUpActiveWebState();
  mediator_.webStateList = browser_->GetWebStateList();

  // Force creation of the model.
  [mediator_ overflowMenuModel];

  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);

  EXPECT_TRUE(HasItem(kToolsMenuNewTabId, /*enabled=*/YES));
  EXPECT_TRUE(HasItem(kToolsMenuSiteInformation, /*enabled=*/YES));
}

// Tests that the items returned by the mediator are correctly enabled on the
// NTP.
TEST_F(OverflowMenuMediatorTest, TestItemsStatusOnNTP) {
  CreateMediator(/*is_incognito=*/NO);
  SetUpActiveWebState();
  mediator_.webStateList = browser_->GetWebStateList();

  // Force creation of the model.
  [mediator_ overflowMenuModel];

  navigation_item_->SetURL(GURL("chrome://newtab"));
  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);

  EXPECT_TRUE(HasItem(kToolsMenuNewTabId, /*enabled=*/YES));
  EXPECT_FALSE(HasItem(kToolsMenuSiteInformation, /*enabled=*/YES));
}

// Tests that the "Add to Reading List" button is disabled while overlay UI is
// displayed in OverlayModality::kWebContentArea.
TEST_F(OverflowMenuMediatorTest, TestReadLaterDisabled) {
  const GURL kUrl("https://chromium.test");
  web_state_->SetCurrentURL(kUrl);
  CreatePrefs();
  CreateMediator(/*is_incognito=*/NO);
  SetUpActiveWebState();
  mediator_.webStateList = browser_->GetWebStateList();
  mediator_.webContentAreaOverlayPresenter = OverlayPresenter::FromBrowser(
      browser_.get(), OverlayModality::kWebContentArea);
  mediator_.prefService = prefs_.get();

  // Force creation of the model.
  [mediator_ overflowMenuModel];

  ASSERT_TRUE(HasItem(kToolsMenuReadLater, /*enabled=*/YES));

  // Present a JavaScript alert over the WebState and verify that the page is no
  // longer shareable.
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state_, OverlayModality::kWebContentArea);
  queue->AddRequest(OverlayRequest::CreateWithConfig<JavaScriptDialogRequest>(
      web::JAVASCRIPT_DIALOG_TYPE_ALERT, web_state_, kUrl,
      /*is_main_frame=*/true, @"message",
      /*default_text_field_value=*/nil));
  EXPECT_TRUE(HasItem(kToolsMenuReadLater, /*enabled=*/NO));

  // Cancel the request and verify that the "Add to Reading List" button is
  // enabled.
  queue->CancelAllRequests();
  EXPECT_TRUE(HasItem(kToolsMenuReadLater, /*enabled=*/YES));
}

// Tests that the "Text Zoom..." button is disabled on non-HTML pages.
TEST_F(OverflowMenuMediatorTest, TestTextZoomDisabled) {
  CreateMediator(/*is_incognito=*/NO);
  SetUpActiveWebState();
  mediator_.webStateList = browser_->GetWebStateList();

  FontSizeTabHelper::CreateForWebState(
      browser_->GetWebStateList()->GetWebStateAt(0));

  // Force creation of the model.
  [mediator_ overflowMenuModel];

  EXPECT_TRUE(HasItem(kToolsMenuTextZoom, /*enabled=*/YES));

  web_state_->SetContentIsHTML(false);
  // Fake a navigationFinished to force the popup menu items to update.
  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);
  EXPECT_TRUE(HasItem(kToolsMenuTextZoom, /*enabled=*/NO));
}

// Tests that the "Managed by..." item is hidden when none of the policies is
// set.
TEST_F(OverflowMenuMediatorTest, TestEnterpriseInfoHidden) {
  CreateMediator(/*is_incognito=*/NO);
  SetUpActiveWebState();

  mediator_.webStateList = browser_->GetWebStateList();

  // Force creation of the model.
  [mediator_ overflowMenuModel];

  ASSERT_FALSE(HasEnterpriseInfoItem());
}

// Tests that the "Managed by..." item is shown.
TEST_F(OverflowMenuMediatorTest, TestEnterpriseInfoShown) {
  // Set a policy.
  base::ScopedTempDir state_directory;
  ASSERT_TRUE(state_directory.CreateUniqueTempDir());

  std::unique_ptr<EnterprisePolicyTestHelper> enterprise_policy_helper =
      std::make_unique<EnterprisePolicyTestHelper>(state_directory.GetPath());
  BrowserPolicyConnectorIOS* connector =
      enterprise_policy_helper->GetBrowserPolicyConnector();

  policy::PolicyMap map;
  map.Set("test-policy", policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
          base::Value("hello"), nullptr);
  enterprise_policy_helper->GetPolicyProvider()->UpdateChromePolicy(map);

  CreateMediatorWithBrowserPolicyConnector(
      /*is_incognito=*/NO, connector);

  SetUpActiveWebState();

  mediator_.webStateList = browser_->GetWebStateList();

  // Force creation of the model.
  [mediator_ overflowMenuModel];

  ASSERT_TRUE(HasEnterpriseInfoItem());
}

// Tests that 1) the tools menu has an enabled 'Add to Bookmarks' button when
// the current URL is not in bookmarks 2) the bookmark button changes to an
// enabled 'Edit bookmark' button when navigating to a bookmarked URL, 3) the
// bookmark button changes to 'Add to Bookmarks' when the bookmark is removed.
TEST_F(OverflowMenuMediatorTest, TestBookmarksToolsMenuButtons) {
  const GURL nonBookmarkedURL("https://nonbookmarked.url");
  const GURL bookmarkedURL("https://bookmarked.url");

  web_state_->SetCurrentURL(nonBookmarkedURL);
  SetUpActiveWebState();

  CreateMediator(/*is_incognito=*/NO);
  CreatePrefs();
  SetUpBookmarks();
  bookmarks::AddIfNotBookmarked(bookmark_model_, bookmarkedURL,
                                base::SysNSStringToUTF16(@"Test bookmark"));
  mediator_.webStateList = browser_->GetWebStateList();
  mediator_.prefService = prefs_.get();

  // Force creation of the model.
  [mediator_ overflowMenuModel];

  EXPECT_TRUE(HasItem(kToolsMenuAddToBookmarks, /*enabled=*/YES));

  // Navigate to bookmarked url
  web_state_->SetCurrentURL(bookmarkedURL);
  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);

  EXPECT_FALSE(HasItem(kToolsMenuAddToBookmarks, /*enabled=*/YES));
  EXPECT_TRUE(HasItem(kToolsMenuEditBookmark, /*enabled=*/YES));

  bookmark_model_->RemoveAllUserBookmarks();
  EXPECT_TRUE(HasItem(kToolsMenuAddToBookmarks, /*enabled=*/YES));
  EXPECT_FALSE(HasItem(kToolsMenuEditBookmark, /*enabled=*/YES));
}

// Tests that the bookmark button is disabled when EditBookmarksEnabled pref is
// changed to false.
TEST_F(OverflowMenuMediatorTest, TestDisableBookmarksButton) {
  const GURL url("https://chromium.test");
  web_state_->SetCurrentURL(url);
  SetUpActiveWebState();

  CreateMediator(/*is_incognito=*/NO);
  CreatePrefs();
  mediator_.webStateList = browser_->GetWebStateList();
  mediator_.prefService = prefs_.get();

  // Force creation of the model.
  [mediator_ overflowMenuModel];

  EXPECT_TRUE(HasItem(kToolsMenuAddToBookmarks, /*enabled=*/YES));

  prefs_->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, false);
  EXPECT_TRUE(HasItem(kToolsMenuAddToBookmarks, /*enabled=*/NO));
}
