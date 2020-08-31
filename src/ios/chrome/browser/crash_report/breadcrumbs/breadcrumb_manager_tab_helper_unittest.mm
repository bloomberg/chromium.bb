// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_tab_helper.h"

#include "base/strings/string_split.h"
#include "base/test/task_environment.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/infobars/test/fake_infobar_delegate.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/ui/infobars/test/fake_infobar_ui_delegate.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/test/error_test_util.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBarDelegate;

// Test fixture for BreadcrumbManagerTabHelper class.
class BreadcrumbManagerTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    first_web_state_.SetBrowserState(chrome_browser_state_.get());
    second_web_state_.SetBrowserState(chrome_browser_state_.get());

    breadcrumb_service_ = static_cast<BreadcrumbManagerKeyedService*>(
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));

    // Navigation manager is needed for InfobarManager.
    first_web_state_.SetNavigationManager(
        std::make_unique<web::TestNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(&first_web_state_);
    second_web_state_.SetNavigationManager(
        std::make_unique<web::TestNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(&second_web_state_);

    BreadcrumbManagerTabHelper::CreateForWebState(&first_web_state_);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  web::TestWebState first_web_state_;
  web::TestWebState second_web_state_;
  BreadcrumbManagerKeyedService* breadcrumb_service_;
};

// Tests that the identifier returned for a WebState is unique.
TEST_F(BreadcrumbManagerTabHelperTest, UniqueIdentifiers) {
  BreadcrumbManagerTabHelper::CreateForWebState(&second_web_state_);

  int first_tab_identifier =
      BreadcrumbManagerTabHelper::FromWebState(&first_web_state_)
          ->GetUniqueId();
  int second_tab_identifier =
      BreadcrumbManagerTabHelper::FromWebState(&second_web_state_)
          ->GetUniqueId();

  EXPECT_GT(first_tab_identifier, 0);
  EXPECT_GT(second_tab_identifier, 0);
  EXPECT_NE(first_tab_identifier, second_tab_identifier);
}

// Tests that BreadcrumbManagerTabHelper events are logged to the associated
// BreadcrumbManagerKeyedService. This test does not attempt to validate that
// every observer method is correctly called as that is done in the
// WebStateObserverTest tests.
TEST_F(BreadcrumbManagerTabHelperTest, EventsLogged) {

  EXPECT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());
  web::FakeNavigationContext context;
  first_web_state_.OnNavigationStarted(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidStartNavigation))
      << events.back();

  first_web_state_.OnNavigationFinished(&context);
  events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidFinishNavigation))
      << events.back();
}

// Tests that BreadcrumbManagerTabHelper events logged from seperate WebStates
// are unique.
TEST_F(BreadcrumbManagerTabHelperTest, UniqueEvents) {
  web::FakeNavigationContext context;
  first_web_state_.OnNavigationStarted(&context);

  BreadcrumbManagerTabHelper::CreateForWebState(&second_web_state_);
  second_web_state_.OnNavigationStarted(&context);

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_STRNE(events.front().c_str(), events.back().c_str());
  EXPECT_NE(std::string::npos,
            events.front().find(kBreadcrumbDidStartNavigation))
      << events.front();
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidStartNavigation))
      << events.back();
}

// Tests metadata for chrome://newtab NTP navigation.
TEST_F(BreadcrumbManagerTabHelperTest, ChromeNewTabNavigationStart) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  web::FakeNavigationContext context;
  context.SetUrl(GURL(kChromeUINewTabURL));
  first_web_state_.OnNavigationStarted(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.front().find(base::StringPrintf(
                                   "%s%lld", kBreadcrumbDidStartNavigation,
                                   context.GetNavigationId())))
      << events.front();
  EXPECT_NE(std::string::npos, events.front().find(kBreadcrumbNtpNavigation))
      << events.front();
}

// Tests metadata for about://newtab NTP navigation.
TEST_F(BreadcrumbManagerTabHelperTest, AboutNewTabNavigationStart) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  web::FakeNavigationContext context;
  context.SetUrl(GURL("about://newtab"));
  first_web_state_.OnNavigationStarted(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.front().find(base::StringPrintf(
                                   "%s%lld", kBreadcrumbDidStartNavigation,
                                   context.GetNavigationId())))
      << events.front();
  EXPECT_NE(std::string::npos, events.front().find(kBreadcrumbNtpNavigation))
      << events.front();
}

// Tests metadata for about://newtab/ NTP navigation.
TEST_F(BreadcrumbManagerTabHelperTest, AboutNewTabNavigationStart2) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  web::FakeNavigationContext context;
  context.SetUrl(GURL("about://newtab/"));
  first_web_state_.OnNavigationStarted(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.front().find(base::StringPrintf(
                                   "%s%lld", kBreadcrumbDidStartNavigation,
                                   context.GetNavigationId())))
      << events.front();
  EXPECT_NE(std::string::npos, events.front().find(kBreadcrumbNtpNavigation))
      << events.front();
}

// Tests unique ID in DidStartNavigation and DidStartNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, NavigationUniqueId) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  // DidStartNavigation
  web::FakeNavigationContext context;
  first_web_state_.OnNavigationStarted(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.front().find(base::StringPrintf(
                                   "%s%lld", kBreadcrumbDidStartNavigation,
                                   context.GetNavigationId())))
      << events.front();

  // DidFinishNavigation
  first_web_state_.OnNavigationFinished(&context);
  events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_NE(std::string::npos, events.back().find(base::StringPrintf(
                                   "%s%lld", kBreadcrumbDidFinishNavigation,
                                   context.GetNavigationId())))
      << events.back();
}

// Tests renderer initiated metadata in DidStartNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, RendererInitiatedByUser) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  web::FakeNavigationContext context;
  context.SetIsRendererInitiated(true);
  context.SetHasUserGesture(true);
  first_web_state_.OnNavigationStarted(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find("#link")) << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidStartNavigation))
      << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbRendererInitiatedByUser))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(kBreadcrumbRendererInitiatedByScript))
      << events.back();
}

// Tests renderer initiated metadata in DidStartNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, RendererInitiatedByScript) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  web::FakeNavigationContext context;
  context.SetIsRendererInitiated(true);
  context.SetHasUserGesture(false);
  context.SetPageTransition(ui::PAGE_TRANSITION_RELOAD);
  first_web_state_.OnNavigationStarted(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find("#reload")) << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidStartNavigation))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(kBreadcrumbRendererInitiatedByUser))
      << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbRendererInitiatedByScript))
      << events.back();
}

// Tests browser initiated metadata in DidStartNavigation.
TEST_F(BreadcrumbManagerTabHelperTest, BrowserInitiatedByScript) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  web::FakeNavigationContext context;
  context.SetIsRendererInitiated(false);
  context.SetPageTransition(ui::PAGE_TRANSITION_TYPED);
  first_web_state_.OnNavigationStarted(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find("#typed")) << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidStartNavigation))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(kBreadcrumbRendererInitiatedByUser))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(kBreadcrumbRendererInitiatedByScript))
      << events.back();
}

// Tests download navigation.
TEST_F(BreadcrumbManagerTabHelperTest, Download) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  web::FakeNavigationContext context;
  context.SetIsDownload(true);
  first_web_state_.OnNavigationFinished(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidFinishNavigation))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbDownload))
      << events.back();
}

// Tests page load succeess.
TEST_F(BreadcrumbManagerTabHelperTest, PageLoadSuccess) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  first_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_EQ(std::string::npos, events.back().find(kBreadcrumbPageLoadFailure))
      << events.back();
}

// Tests page load failure.
TEST_F(BreadcrumbManagerTabHelperTest, PageLoadFailure) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  first_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbPageLoadFailure))
      << events.back();
}

// Tests NTP page load.
TEST_F(BreadcrumbManagerTabHelperTest, NtpPageLoad) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  first_web_state_.SetCurrentURL(GURL(kChromeUINewTabURL));
  first_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbNtpNavigation))
      << events.back();
  // NTP navigation can't fail, so there is no success/failure metadata.
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbPageLoaded))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbNtpNavigation))
      << events.back();
}

// Tests navigation error.
TEST_F(BreadcrumbManagerTabHelperTest, NavigationError) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  web::FakeNavigationContext context;
  NSError* error = web::testing::CreateTestNetError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorNotConnectedToInternet
             userInfo:nil]);
  context.SetError(error);
  first_web_state_.OnNavigationFinished(&context);
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbDidFinishNavigation))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(net::ErrorToShortString(
                                   net::ERR_INTERNET_DISCONNECTED)))
      << events.back();
}

// Tests changes in security states.
TEST_F(BreadcrumbManagerTabHelperTest, DidChangeVisibleSecurityState) {
  auto navigation_manager = std::make_unique<web::TestNavigationManager>();
  web::TestNavigationManager* navigation_manager_ptr = navigation_manager.get();
  first_web_state_.SetNavigationManager(std::move(navigation_manager));
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  // Empty navigation manager.
  first_web_state_.OnVisibleSecurityStateChanged();
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  // Default navigation item.
  auto visible_item = web::NavigationItem::Create();
  navigation_manager_ptr->SetVisibleItem(visible_item.get());
  first_web_state_.OnVisibleSecurityStateChanged();
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  // Mixed content.
  web::SSLStatus& status = visible_item->GetSSL();
  status.content_status = web::SSLStatus::DISPLAYED_INSECURE_CONTENT;
  first_web_state_.OnVisibleSecurityStateChanged();
  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbMixedContent))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(kBreadcrumbAuthenticationBroken))
      << events.back();

  // Broken authentication.
  status.content_status = web::SSLStatus::NORMAL_CONTENT;
  status.security_style = web::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  first_web_state_.OnVisibleSecurityStateChanged();
  events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_EQ(std::string::npos, events.back().find(kBreadcrumbMixedContent))
      << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbAuthenticationBroken))
      << events.back();
}

// Tests that adding an infobar logs the expected breadcrumb.
TEST_F(BreadcrumbManagerTabHelperTest, AddInfobar) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  InfoBarDelegate::InfoBarIdentifier identifier =
      InfoBarDelegate::InfoBarIdentifier::SESSION_CRASHED_INFOBAR_DELEGATE_IOS;
  std::unique_ptr<FakeInfobarDelegate> delegate =
      std::make_unique<FakeInfobarDelegate>(identifier);
  std::unique_ptr<FakeInfobarIOS> infobar =
      std::make_unique<FakeInfobarIOS>(std::move(delegate));
  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::move(infobar));

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(base::StringPrintf(
                "%s%d", kBreadcrumbInfobarAdded, identifier)))
      << events.back();
}

// Tests that infobar breadcrumbs specify the infobar type.
TEST_F(BreadcrumbManagerTabHelperTest, InfobarTypes) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  // Add and remove first infobar.
  InfoBarDelegate::InfoBarIdentifier first_identifier =
      InfoBarDelegate::InfoBarIdentifier::SESSION_CRASHED_INFOBAR_DELEGATE_IOS;
  std::unique_ptr<FakeInfobarDelegate> first_delegate =
      std::make_unique<FakeInfobarDelegate>(first_identifier);
  std::unique_ptr<FakeInfobarIOS> first_infobar =
      std::make_unique<FakeInfobarIOS>(std::move(first_delegate));
  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::move(first_infobar));
  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->RemoveAllInfoBars(/*animate=*/false);

  // Add second infobar.
  InfoBarDelegate::InfoBarIdentifier second_identifier =
      InfoBarDelegate::InfoBarIdentifier::SYNC_ERROR_INFOBAR_DELEGATE_IOS;
  std::unique_ptr<FakeInfobarDelegate> second_delegate =
      std::make_unique<FakeInfobarDelegate>(second_identifier);
  std::unique_ptr<FakeInfobarIOS> second_infobar =
      std::make_unique<FakeInfobarIOS>(std::move(second_delegate));
  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::move(second_infobar));

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(3ul, events.size());
  EXPECT_NE(events.front(), events.back());
  EXPECT_NE(std::string::npos,
            events.front().find(base::StringPrintf(
                "%s%d", kBreadcrumbInfobarAdded, first_identifier)))
      << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(base::StringPrintf(
                "%s%d", kBreadcrumbInfobarAdded, second_identifier)))
      << events.back();
}

// Tests that removing an infobar without animation logs the expected breadcrumb
// event.
TEST_F(BreadcrumbManagerTabHelperTest, RemoveInfobarNotAnimated) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  InfoBarDelegate::InfoBarIdentifier identifier =
      InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR;
  std::unique_ptr<FakeInfobarDelegate> delegate =
      std::make_unique<FakeInfobarDelegate>(identifier);
  std::unique_ptr<FakeInfobarIOS> infobar =
      std::make_unique<FakeInfobarIOS>(std::move(delegate));
  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::move(infobar));

  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->RemoveAllInfoBars(/*animate=*/false);

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(base::StringPrintf(
                "%s%d", kBreadcrumbInfobarRemoved, identifier)))
      << events.back();
  EXPECT_NE(std::string::npos,
            events.back().find(kBreadcrumbInfobarNotAnimated))
      << events.back();
}

// Tests that removing an infobar with animation logs the expected breadcrumb
// event.
TEST_F(BreadcrumbManagerTabHelperTest, RemoveInfobarAnimated) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  InfoBarDelegate::InfoBarIdentifier identifier =
      InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR;
  std::unique_ptr<FakeInfobarDelegate> delegate =
      std::make_unique<FakeInfobarDelegate>(identifier);
  std::unique_ptr<FakeInfobarIOS> infobar =
      std::make_unique<FakeInfobarIOS>(std::move(delegate));
  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::move(infobar));

  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->RemoveAllInfoBars(/*animate=*/true);

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_NE(std::string::npos,
            events.back().find(base::StringPrintf(
                "%s%d", kBreadcrumbInfobarRemoved, identifier)))
      << events.back();
  EXPECT_EQ(std::string::npos,
            events.back().find(kBreadcrumbInfobarNotAnimated))
      << events.back();
}

// Tests that replacing an infobar logs the expected breadcrumb event.
TEST_F(BreadcrumbManagerTabHelperTest, ReplaceInfobar) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::make_unique<FakeInfobarIOS>());

  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::make_unique<FakeInfobarIOS>(),
                   /*replace_existing=*/true);

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(2ul, events.size());

  InfoBarDelegate::InfoBarIdentifier identifier =
      InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR;
  EXPECT_NE(std::string::npos,
            events.back().find(base::StringPrintf(
                "%s%d", kBreadcrumbInfobarReplaced, identifier)))
      << events.back();
}

// Tests that replacing an infobar many times only logs the replaced infobar
// breadcrumb at major increments.
TEST_F(BreadcrumbManagerTabHelperTest, SequentialInfobarReplacements) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  InfoBarManagerImpl::FromWebState(&first_web_state_)
      ->AddInfoBar(std::make_unique<FakeInfobarIOS>());

  for (int replacements = 0; replacements < 500; replacements++) {
    InfoBarManagerImpl::FromWebState(&first_web_state_)
        ->AddInfoBar(std::make_unique<FakeInfobarIOS>(),
                     /*replace_existing=*/true);
  }

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  // Replacing the infobar 500 times should only log breadcrumbs on the 1st,
  // 2nd, 5th, 20th, 100th, 200th replacement.
  ASSERT_EQ(7ul, events.size());

  // The events should contain the number of times the info has been replaced.
  // Validate the last one, which occurs at the 200th replacement.
  std::string expected_event =
      base::StringPrintf("%s%d %d", kBreadcrumbInfobarReplaced,
                         InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR, 200);
  EXPECT_NE(std::string::npos, events.back().find(expected_event))
      << events.back();
}
