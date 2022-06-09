// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper.h"

#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "components/policy/policy_constants.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper_delegate.h"
#import "ios/chrome/browser/app_launcher/fake_app_launcher_abuse_detector.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_switches.h"
#import "ios/chrome/browser/chrome_url_util.h"
#import "ios/chrome/browser/policy/enterprise_policy_test_helper.h"
#import "ios/chrome/browser/policy/policy_features.h"
#include "ios/chrome/browser/policy_url_blocking/policy_url_blocking_service.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/u2f/u2f_tab_helper.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// An fake AppLauncherTabHelperDelegate for tests.
class FakeAppLauncherTabHelperDelegate : public AppLauncherTabHelperDelegate {
 public:
  GURL last_launched_app_url() { return last_launched_app_url_; }
  size_t app_launch_count() { return app_launch_count_; }
  size_t alert_shown_count() { return alert_shown_count_; }
  void set_should_accept_prompt(bool should_accept_prompt) {
    should_accept_prompt_ = should_accept_prompt;
  }
  bool should_accept_prompt() { return should_accept_prompt_; }

  // AppLauncherTabHelperDelegate:
  void LaunchAppForTabHelper(AppLauncherTabHelper* tab_helper,
                             const GURL& url,
                             bool link_transition) override {
    ++app_launch_count_;
    last_launched_app_url_ = url;
  }
  void ShowRepeatedAppLaunchAlert(
      AppLauncherTabHelper* tab_helper,
      base::OnceCallback<void(bool)> completion) override {
    ++alert_shown_count_;
    std::move(completion).Run(should_accept_prompt_);
  }

 private:
  // URL of the last launched application.
  GURL last_launched_app_url_;
  // Number of times an app was launched.
  size_t app_launch_count_ = 0;
  // Number of times the repeated launches alert has been shown.
  size_t alert_shown_count_ = 0;
  // Simulates the user tapping the accept button when prompted via
  // |-appLauncherTabHelper:showAlertOfRepeatedLaunchesWithCompletionHandler|.
  bool should_accept_prompt_ = false;
};
// A fake NavigationManager to be used by the WebState object for the
// AppLauncher.
class FakeNavigationManager : public web::FakeNavigationManager {
 public:
  FakeNavigationManager() = default;

  FakeNavigationManager(const FakeNavigationManager&) = delete;
  FakeNavigationManager& operator=(const FakeNavigationManager&) = delete;

  // web::NavigationManager implementation.
  void DiscardNonCommittedItems() override {}
};

std::unique_ptr<KeyedService> BuildReadingListModel(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  std::unique_ptr<ReadingListModelImpl> reading_list_model(
      new ReadingListModelImpl(nullptr, browser_state->GetPrefs(),
                               base::DefaultClock::GetInstance()));
  return reading_list_model;
}
}  // namespace

// Test fixture for AppLauncherTabHelper class.
class AppLauncherTabHelperTest : public PlatformTest {
 protected:
  AppLauncherTabHelperTest()
      : abuse_detector_([[FakeAppLauncherAbuseDetector alloc] init]) {
    AppLauncherTabHelper::CreateForWebState(&web_state_, abuse_detector_);
    U2FTabHelper::CreateForWebState(&web_state_);
    // Allow is the default policy for this test.
    abuse_detector_.policy = ExternalAppLaunchPolicyAllow;
    auto navigation_manager = std::make_unique<FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    web_state_.SetCurrentURL(GURL("https://chromium.org"));
    tab_helper_ = AppLauncherTabHelper::FromWebState(&web_state_);
    tab_helper_->SetDelegate(&delegate_);
  }

  bool TestShouldAllowRequest(NSString* url_string,
                              bool target_frame_is_main,
                              bool target_frame_is_cross_origin,
                              bool has_user_gesture,
                              ui::PageTransition transition_type =
                                  ui::PageTransition::PAGE_TRANSITION_LINK)
      WARN_UNUSED_RESULT {
    NSURL* url = [NSURL URLWithString:url_string];
    const web::WebStatePolicyDecider::RequestInfo request_info(
        transition_type, target_frame_is_main, target_frame_is_cross_origin,
        has_user_gesture);
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    auto callback =
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          policy_decision = decision;
          callback_called = true;
        });
    tab_helper_->ShouldAllowRequest([NSURLRequest requestWithURL:url],
                                    request_info, std::move(callback));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(callback_called);
    return policy_decision.ShouldAllowNavigation();
  }

  // Initialize reading list model and its required tab helpers.
  void InitializeReadingListModel() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    web_state_.SetBrowserState(chrome_browser_state_.get());
    ReadingListModelFactory::GetInstance()->SetTestingFactoryAndUse(
        chrome_browser_state_.get(),
        base::BindRepeating(&BuildReadingListModel));
    TabIdTabHelper::CreateForWebState(&web_state_);
    is_reading_list_initialized_ = true;
  }

  // Returns true if the |expected_read_status| matches the read status for any
  // non empty source URL based on the transition type and the app policy.
  bool TestReadingListUpdate(bool is_app_blocked,
                             bool is_link_transition,
                             bool expected_read_status) {
    // Make sure reading list model is initialized.
    if (!is_reading_list_initialized_)
      InitializeReadingListModel();

    web_state_.SetCurrentURL(GURL("https://chromium.org"));
    GURL pending_url("http://google.com");
    navigation_manager_->AddItem(pending_url, ui::PAGE_TRANSITION_LINK);
    web::NavigationItem* item = navigation_manager_->GetItemAtIndex(0);
    navigation_manager_->SetPendingItem(item);
    item->SetOriginalRequestURL(pending_url);

    ReadingListModel* model = ReadingListModelFactory::GetForBrowserState(
        chrome_browser_state_.get());
    EXPECT_TRUE(model->DeleteAllEntries());
    model->AddEntry(pending_url, "unread", reading_list::ADDED_VIA_CURRENT_APP);
    abuse_detector_.policy = is_app_blocked ? ExternalAppLaunchPolicyBlock
                                            : ExternalAppLaunchPolicyAllow;
    ui::PageTransition transition_type =
        is_link_transition ? ui::PageTransition::PAGE_TRANSITION_LINK
                           : ui::PageTransition::PAGE_TRANSITION_TYPED;

    NSURL* url = [NSURL
        URLWithString:@"itms-apps://itunes.apple.com/us/app/appname/id123"];
    const web::WebStatePolicyDecider::RequestInfo request_info(
        transition_type,
        /*target_frame_is_main=*/true, /*target_frame_is_cross_origin=*/false,
        /*has_user_gesture=*/true);
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    auto callback =
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          policy_decision = decision;
          callback_called = true;
        });
    tab_helper_->ShouldAllowRequest([NSURLRequest requestWithURL:url],
                                    request_info, std::move(callback));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(policy_decision.ShouldCancelNavigation());

    const ReadingListEntry* entry = model->GetEntryByURL(pending_url);
    return entry->IsRead() == expected_read_status;
  }

  base::test::TaskEnvironment task_environment;
  web::FakeWebState web_state_;
  FakeNavigationManager* navigation_manager_ = nullptr;

  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_ = nil;
  FakeAppLauncherAbuseDetector* abuse_detector_ = nil;
  FakeAppLauncherTabHelperDelegate delegate_;
  bool is_reading_list_initialized_ = false;
  AppLauncherTabHelper* tab_helper_ = nullptr;
};

// Tests that a valid URL launches app.
// TODO(crbug.com/1172516): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_AbuseDetectorPolicyAllowedForValidUrl \
  AbuseDetectorPolicyAllowedForValidUrl
#else
#define MAYBE_AbuseDetectorPolicyAllowedForValidUrl \
  DISABLED_AbuseDetectorPolicyAllowedForValidUrl
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_AbuseDetectorPolicyAllowedForValidUrl) {
  abuse_detector_.policy = ExternalAppLaunchPolicyAllow;
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(1U, delegate_.app_launch_count());
  EXPECT_EQ(GURL("valid://1234"), delegate_.last_launched_app_url());
}

// Tests that a valid URL does not launch app when launch policy is to block.
TEST_F(AppLauncherTabHelperTest, AbuseDetectorPolicyBlockedForValidUrl) {
  abuse_detector_.policy = ExternalAppLaunchPolicyBlock;
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(0U, delegate_.alert_shown_count());
  EXPECT_EQ(0U, delegate_.app_launch_count());
}

// Tests that a valid URL shows an alert and launches app when launch policy is
// to prompt and user accepts.
// TODO(crbug.com/1172516): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ValidUrlPromptUserAccepts ValidUrlPromptUserAccepts
#else
#define MAYBE_ValidUrlPromptUserAccepts DISABLED_ValidUrlPromptUserAccepts
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_ValidUrlPromptUserAccepts) {
  abuse_detector_.policy = ExternalAppLaunchPolicyPrompt;
  delegate_.set_should_accept_prompt(true);
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));

  EXPECT_EQ(1U, delegate_.alert_shown_count());
  EXPECT_EQ(1U, delegate_.app_launch_count());
  EXPECT_EQ(GURL("valid://1234"), delegate_.last_launched_app_url());
}

// Tests that a valid URL does not launch app when launch policy is to prompt
// and user rejects.
TEST_F(AppLauncherTabHelperTest, ValidUrlPromptUserRejects) {
  abuse_detector_.policy = ExternalAppLaunchPolicyPrompt;
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(0U, delegate_.app_launch_count());
}

// Tests that ShouldAllowRequest only launches apps for App Urls in main frame,
// or iframe when there was a recent user interaction.
// TODO(crbug.com/1172516): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ShouldAllowRequestWithAppUrl ShouldAllowRequestWithAppUrl
#else
#define MAYBE_ShouldAllowRequestWithAppUrl DISABLED_ShouldAllowRequestWithAppUrl
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_ShouldAllowRequestWithAppUrl) {
  NSString* url_string = @"itms-apps://itunes.apple.com/us/app/appname/id123";
  EXPECT_FALSE(TestShouldAllowRequest(url_string, /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(1U, delegate_.app_launch_count());

  EXPECT_FALSE(TestShouldAllowRequest(url_string, /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/true));
  EXPECT_EQ(2U, delegate_.app_launch_count());

  EXPECT_FALSE(TestShouldAllowRequest(url_string,
                                      /*target_frame_is_main=*/false,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(2U, delegate_.app_launch_count());

  EXPECT_FALSE(TestShouldAllowRequest(url_string,
                                      /*target_frame_is_main=*/false,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/true));
  EXPECT_EQ(3U, delegate_.app_launch_count());
}

// Tests that ShouldAllowRequest always allows requests and does not launch
// apps for non App Urls.
TEST_F(AppLauncherTabHelperTest, ShouldAllowRequestWithNonAppUrl) {
  EXPECT_TRUE(TestShouldAllowRequest(
      @"http://itunes.apple.com/us/app/appname/id123",
      /*target_frame_is_main=*/true, /*target_frame_is_cross_origin=*/false,
      /*has_user_gesture=*/false));
  EXPECT_TRUE(TestShouldAllowRequest(@"file://a/b/c",
                                     /*target_frame_is_main=*/true,
                                     /*target_frame_is_cross_origin=*/false,
                                     /*has_user_gesture=*/true));
  EXPECT_TRUE(TestShouldAllowRequest(@"about://test",
                                     /*target_frame_is_main=*/false,
                                     /*target_frame_is_cross_origin=*/false,
                                     /*has_user_gesture=*/false));
  EXPECT_TRUE(TestShouldAllowRequest(@"data://test",
                                     /*target_frame_is_main=*/false,
                                     /*target_frame_is_cross_origin=*/false,
                                     /*has_user_gesture=*/true));
  EXPECT_TRUE(TestShouldAllowRequest(@"blob://test",
                                     /*target_frame_is_main=*/false,
                                     /*target_frame_is_cross_origin=*/false,
                                     /*has_user_gesture=*/true));
  EXPECT_EQ(0U, delegate_.app_launch_count());
}

// Tests that invalid Urls are completely blocked.
TEST_F(AppLauncherTabHelperTest, InvalidUrls) {
  EXPECT_FALSE(TestShouldAllowRequest(/*url_string=*/@"",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_FALSE(TestShouldAllowRequest(@"invalid",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(0U, delegate_.app_launch_count());
}

// Tests that when the last committed URL is invalid, the URL is only opened
// when the last committed item is nil.
// TODO(crbug.com/1172516): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ValidUrlInvalidCommittedURL ValidUrlInvalidCommittedURL
#else
#define MAYBE_ValidUrlInvalidCommittedURL DISABLED_ValidUrlInvalidCommittedURL
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_ValidUrlInvalidCommittedURL) {
  NSString* url_string = @"itms-apps://itunes.apple.com/us/app/appname/id123";
  web_state_.SetCurrentURL(GURL());

  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  item->SetURL(GURL());

  navigation_manager_->SetLastCommittedItem(item.get());
  EXPECT_FALSE(TestShouldAllowRequest(url_string,
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(0U, delegate_.app_launch_count());

  navigation_manager_->SetLastCommittedItem(nullptr);
  EXPECT_FALSE(TestShouldAllowRequest(url_string,
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(1U, delegate_.app_launch_count());
}

// Tests that URLs with schemes that might be a security risk are blocked.
TEST_F(AppLauncherTabHelperTest, InsecureUrls) {
  EXPECT_FALSE(TestShouldAllowRequest(@"app-settings://",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(0U, delegate_.app_launch_count());
}

// Tests that tel: URLs are blocked when the target frame is cross-origin
// with respect to the source origin.
// TODO(crbug.com/1172516): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_TelUrls TelUrls
#else
#define MAYBE_TelUrls DISABLED_TelUrls
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_TelUrls) {
  EXPECT_FALSE(TestShouldAllowRequest(@"tel:+12345551212",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/true,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(0U, delegate_.app_launch_count());

  EXPECT_FALSE(TestShouldAllowRequest(@"tel:+12345551212",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/true,
                                      /*has_user_gesture=*/true));
  EXPECT_EQ(0U, delegate_.app_launch_count());

  EXPECT_FALSE(TestShouldAllowRequest(@"tel:+12345551212",
                                      /*target_frame_is_main=*/false,
                                      /*target_frame_is_cross_origin=*/true,
                                      /*has_user_gesture=*/true));
  EXPECT_EQ(0U, delegate_.app_launch_count());

  EXPECT_FALSE(TestShouldAllowRequest(@"tel:+12345551212",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(1U, delegate_.app_launch_count());
}

// Tests that URLs with U2F schemes are handled correctly.
// This test is using https://chromeiostesting-dot-u2fdemo.appspot.com URL which
// is a URL allowed for the purpose of testing, but the test doesn't send any
// requests to the server.
// TODO(crbug.com/1172516): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_U2FUrls U2FUrls
#else
#define MAYBE_U2FUrls DISABLED_U2FUrls
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_U2FUrls) {
  // Add required tab helpers for the U2F check.
  TabIdTabHelper::CreateForWebState(&web_state_);
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();

  // "u2f-x-callback" scheme should only be created by the browser. External
  // URLs with that scheme should be blocked to prevent malicious sites from
  // bypassing the browser origin/security check for u2f schemes.
  item->SetURL(GURL("https://chromeiostesting-dot-u2fdemo.appspot.com"));
  navigation_manager_->SetLastCommittedItem(item.get());
  EXPECT_FALSE(TestShouldAllowRequest(@"u2f-x-callback://chromium.test",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(0U, delegate_.app_launch_count());

  // Source URL is not trusted, so u2f scheme should not be allowed.
  item->SetURL(GURL("https://chromium.test"));
  navigation_manager_->SetLastCommittedItem(item.get());
  EXPECT_FALSE(TestShouldAllowRequest(@"u2f://chromium.test",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(0U, delegate_.app_launch_count());

  // Source URL is trusted, so u2f scheme should be allowed and an external app
  // is launched via URL with u2f-x-callback scheme.
  item->SetURL(GURL("https://chromeiostesting-dot-u2fdemo.appspot.com"));
  navigation_manager_->SetLastCommittedItem(item.get());
  EXPECT_FALSE(TestShouldAllowRequest(@"u2f://chromium.test",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(1U, delegate_.app_launch_count());
  EXPECT_TRUE(delegate_.last_launched_app_url().SchemeIs("u2f-x-callback"));
}

// Tests that URLs with Chrome Bundle schemes are blocked on iframes.
// TODO(crbug.com/1172516): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ChromeBundleUrlScheme ChromeBundleUrlScheme
#else
#define MAYBE_ChromeBundleUrlScheme DISABLED_ChromeBundleUrlScheme
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_ChromeBundleUrlScheme) {
  // Get the test bundle URL Scheme.
  NSString* scheme = [[ChromeAppConstants sharedInstance] bundleURLScheme];
  NSString* url = [NSString stringWithFormat:@"%@://www.google.com", scheme];
  EXPECT_FALSE(TestShouldAllowRequest(url,
                                      /*target_frame_is_main=*/false,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(0U, delegate_.app_launch_count());

  EXPECT_FALSE(TestShouldAllowRequest(url,
                                      /*target_frame_is_main=*/false,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/true));
  EXPECT_EQ(0U, delegate_.app_launch_count());

  // Chrome Bundle URL scheme is only allowed from main frames.
  EXPECT_FALSE(TestShouldAllowRequest(url,
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(1U, delegate_.app_launch_count());
}

// Tests that ShouldAllowRequest updates the reading list correctly for non-link
// transitions regardless of the app launching success when AppLauncherRefresh
// flag is enabled.
// TODO(crbug.com/1172516): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_UpdatingTheReadingList UpdatingTheReadingList
#else
#define MAYBE_UpdatingTheReadingList DISABLED_UpdatingTheReadingList
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_UpdatingTheReadingList) {
  // Update reading list if the transition is not a link transition.
  EXPECT_TRUE(TestReadingListUpdate(/*is_app_blocked=*/true,
                                    /*is_link_transition*/ false,
                                    /*expected_read_status*/ true));
  EXPECT_EQ(0U, delegate_.app_launch_count());

  EXPECT_TRUE(TestReadingListUpdate(/*is_app_blocked=*/false,
                                    /*is_link_transition*/ false,
                                    /*expected_read_status*/ true));
  EXPECT_EQ(1U, delegate_.app_launch_count());

  // Don't update reading list if the transition is a link transition.
  EXPECT_TRUE(TestReadingListUpdate(/*is_app_blocked=*/true,
                                    /*is_link_transition*/ true,
                                    /*expected_read_status*/ false));
  EXPECT_EQ(1U, delegate_.app_launch_count());

  EXPECT_TRUE(TestReadingListUpdate(/*is_app_blocked=*/false,
                                    /*is_link_transition*/ true,
                                    /*expected_read_status*/ false));
  EXPECT_EQ(2U, delegate_.app_launch_count());
}

// Tests that launching a SMS URL via a JavaScript redirect in the main frame
// is allowed. Covers the scenario for crbug.com/1058388
// TODO(crbug.com/1172516): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_LaunchSmsApp_JavaScriptRedirect LaunchSmsApp_JavaScriptRedirect
#else
#define MAYBE_LaunchSmsApp_JavaScriptRedirect \
  DISABLED_LaunchSmsApp_JavaScriptRedirect
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_LaunchSmsApp_JavaScriptRedirect) {
  NSString* sms_url_string = @"sms:?&body=Hello%20World";
  ui::PageTransition page_transition = ui::PageTransitionFromInt(
      ui::PageTransition::PAGE_TRANSITION_LINK |
      ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT);
  EXPECT_FALSE(
      TestShouldAllowRequest(sms_url_string, /*target_frame_is_main=*/true,
                             /*target_frame_is_cross_origin=*/false,
                             /*has_user_gesture=*/false, page_transition));
  EXPECT_EQ(1U, delegate_.app_launch_count());
}

// Test fixture for testing the app launcher interaction with enterprise policy
// URLBlocklist.
class BlockedUrlPolicyAppLauncherTabHelperTest
    : public AppLauncherTabHelperTest {
 protected:
  BlockedUrlPolicyAppLauncherTabHelperTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kInstallURLBlocklistHandlers);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableEnterprisePolicy);
  }

  void SetUp() override {
    AppLauncherTabHelperTest::SetUp();

    ASSERT_TRUE(state_directory_.CreateUniqueTempDir());
    enterprise_policy_helper_ = std::make_unique<EnterprisePolicyTestHelper>(
        state_directory_.GetPath());
    ASSERT_TRUE(enterprise_policy_helper_->GetBrowserState());

    web_state_.SetBrowserState(enterprise_policy_helper_->GetBrowserState());

    policy::PolicyMap policy_map;
    base::Value value(base::Value::Type::LIST);
    value.Append("itms-apps://*");
    policy_map.Set(policy::key::kURLBlocklist, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   std::move(value), nullptr);
    enterprise_policy_helper_->GetPolicyProvider()->UpdateChromePolicy(
        policy_map);

    policy_blocklist_service_ = static_cast<PolicyBlocklistService*>(
        PolicyBlocklistServiceFactory::GetForBrowserState(
            enterprise_policy_helper_->GetBrowserState()));
  }

  // Temporary directory to hold preference files.
  base::ScopedTempDir state_directory_;

  // Enterprise policy boilerplate configuration.
  std::unique_ptr<EnterprisePolicyTestHelper> enterprise_policy_helper_;
  PolicyBlocklistService* policy_blocklist_service_;
};

// Tests that URLs to blocked domains do not open native apps.
TEST_F(BlockedUrlPolicyAppLauncherTabHelperTest, BlockedUrl) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{kURLBlocklistIOS},
      /*disabled_features=*/{});

  NSString* url_string = @"itms-apps://itunes.apple.com/us/app/appname/id123";
  EXPECT_FALSE(TestShouldAllowRequest(url_string, /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(0U, delegate_.app_launch_count());
}

// Tests that URLs to non-blocked domains are able to open native apps when
// policy is blocking other domains.
// TODO(crbug.com/1172516): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_AllowedUrl AllowedUrl
#else
#define MAYBE_AllowedUrl DISABLED_AllowedUrl
#endif
TEST_F(BlockedUrlPolicyAppLauncherTabHelperTest, MAYBE_AllowedUrl) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{kURLBlocklistIOS},
      /*disabled_features=*/{});

  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*has_user_gesture=*/false));
  EXPECT_EQ(1U, delegate_.app_launch_count());
  EXPECT_EQ(GURL("valid://1234"), delegate_.last_launched_app_url());
}
