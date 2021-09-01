// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_browser_agent.h"

#include "base/bind.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_tab_helper.h"
#import "ios/chrome/browser/download/confirm_download_replacing_overlay.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/web_content_area/app_launcher_overlay.h"
#import "ios/chrome/browser/overlays/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_overlay.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/ui/java_script_dialog_type.h"
#import "ios/web/public/web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Creates test state, inserts it into WebState list and activates.
void InsertWebState(Browser* browser) {
  auto web_state = std::make_unique<web::FakeWebState>();
  InfoBarManagerImpl::CreateForWebState(web_state.get());
  BreadcrumbManagerTabHelper::CreateForWebState(web_state.get());
  browser->GetWebStateList()->InsertWebState(
      /*index=*/0, std::move(web_state), WebStateList::INSERT_ACTIVATE,
      WebStateOpener());
}
}  // namespace

// Test fixture for testing BreadcrumbManagerBrowserAgent class.
class BreadcrumbManagerBrowserAgentTest : public PlatformTest {
 protected:
  BreadcrumbManagerBrowserAgentTest()
      : web_state_list_(&web_state_list_delegate_) {
    TestChromeBrowserState::Builder test_cbs_builder;
    browser_state_ = test_cbs_builder.Build();

    breadcrumb_service_ =
        static_cast<breadcrumbs::BreadcrumbManagerKeyedService*>(
            BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
                browser_state_.get()));

    browser_ =
        std::make_unique<TestBrowser>(browser_state_.get(), &web_state_list_);

    OverlayPresenter::FromBrowser(browser_.get(),
                                  OverlayModality::kWebContentArea)
        ->SetPresentationContext(&presentation_context_);
  }

  web::WebTaskEnvironment task_env_{
      web::WebTaskEnvironment::Options::DEFAULT,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_service_;
  FakeOverlayPresentationContext presentation_context_;
  std::unique_ptr<Browser> browser_;
};

// Tests that an event logged by the BrowserAgent is returned with events for
// the associated |browser_state_|.
TEST_F(BreadcrumbManagerBrowserAgentTest, LogEvent) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  InsertWebState(browser_.get());

  EXPECT_EQ(1ul, breadcrumb_service_->GetEvents(0).size());
}

// Tests that events logged through BrowserAgents associated with different
// Browser instances are returned with events for the associated
// |browser_state_| and are uniquely identifiable.
TEST_F(BreadcrumbManagerBrowserAgentTest, MultipleBrowsers) {
  ASSERT_EQ(0ul, breadcrumb_service_->GetEvents(0).size());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  // Insert WebState into |browser|.
  InsertWebState(browser_.get());

  // Create and setup second Browser.
  WebStateList web_state_list2(&web_state_list_delegate_);
  std::unique_ptr<Browser> browser2 =
      std::make_unique<TestBrowser>(browser_state_.get(), &web_state_list2);
  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser2.get());

  // Insert WebState into |browser2|.
  InsertWebState(browser2.get());

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  EXPECT_EQ(2ul, events.size());

  // Seperately compare the start and end of the event strings to ensure
  // uniqueness at both the Browser and WebState layer.
  std::size_t browser1_split_pos = events.front().find("Insert");
  std::size_t browser2_split_pos = events.back().find("Insert");
  // The start of the string must be unique to differentiate the associated
  // Browser object by including the BreadcrumbManagerBrowserAgent's
  // |unique_id_|.
  // (The Timestamp will match due to TimeSource::MOCK_TIME in the |task_env_|.)
  std::string browser1_start = events.front().substr(browser1_split_pos);
  std::string browser2_start = events.back().substr(browser2_split_pos);
  EXPECT_STRNE(browser1_start.c_str(), browser2_start.c_str());
  // The end of the string must be unique because the WebStates are different
  // and that needs to be represented in the event string.
  std::string browser1_end = events.front().substr(
      browser1_split_pos, events.front().length() - browser1_split_pos);
  std::string browser2_end = events.back().substr(
      browser2_split_pos, events.back().length() - browser2_split_pos);
  EXPECT_STRNE(browser1_end.c_str(), browser2_end.c_str());
}

// Tests WebStateList's batch insertion and closing.
TEST_F(BreadcrumbManagerBrowserAgentTest, BatchOperations) {
  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  // Insert multiple WebStates.
  web_state_list_.PerformBatchOperation(base::BindOnce(^(WebStateList* list) {
    InsertWebState(browser_.get());
    InsertWebState(browser_.get());
  }));

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());
  EXPECT_NE(std::string::npos, events.front().find("Inserted 2 tabs"))
      << events.front();

  // Close multiple WebStates.
  web_state_list_.PerformBatchOperation(base::BindOnce(^(WebStateList* list) {
    list->CloseWebStateAt(
        /*index=*/0, WebStateList::ClosingFlags::CLOSE_NO_FLAGS);
    list->CloseWebStateAt(
        /*index=*/0, WebStateList::ClosingFlags::CLOSE_NO_FLAGS);
  }));

  events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_NE(std::string::npos, events.back().find("Closed 2 tabs"))
      << events.back();
}

// Tests logging kBreadcrumbOverlayJsAlert.
TEST_F(BreadcrumbManagerBrowserAgentTest, JavaScriptAlertOverlay) {
  InsertWebState(browser_.get());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state_list_.GetWebStateAt(0), OverlayModality::kWebContentArea);
  queue->AddRequest(OverlayRequest::CreateWithConfig<
                    java_script_dialog_overlays::JavaScriptDialogRequest>(
      web::JAVASCRIPT_DIALOG_TYPE_ALERT, web_state_list_.GetWebStateAt(0),
      GURL::EmptyGURL(),
      /*is_main_frame=*/true, @"message",
      /*default_text_field_value=*/nil));
  queue->CancelAllRequests();

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbOverlay))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbOverlayJsAlert))
      << events.back();
}

// Tests logging kBreadcrumbOverlayJsConfirm.
TEST_F(BreadcrumbManagerBrowserAgentTest, JavaScriptConfirmOverlay) {
  InsertWebState(browser_.get());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state_list_.GetWebStateAt(0), OverlayModality::kWebContentArea);
  queue->AddRequest(OverlayRequest::CreateWithConfig<
                    java_script_dialog_overlays::JavaScriptDialogRequest>(
      web::JAVASCRIPT_DIALOG_TYPE_CONFIRM, web_state_list_.GetWebStateAt(0),
      GURL::EmptyGURL(),
      /*is_main_frame=*/true, @"message",
      /*default_text_field_value=*/nil));
  queue->CancelAllRequests();

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbOverlay))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbOverlayJsConfirm))
      << events.back();
}

// Tests logging kBreadcrumbOverlayJsPrompt.
TEST_F(BreadcrumbManagerBrowserAgentTest, JavaScriptPromptOverlay) {
  InsertWebState(browser_.get());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state_list_.GetWebStateAt(0), OverlayModality::kWebContentArea);
  queue->AddRequest(OverlayRequest::CreateWithConfig<
                    java_script_dialog_overlays::JavaScriptDialogRequest>(
      web::JAVASCRIPT_DIALOG_TYPE_PROMPT, web_state_list_.GetWebStateAt(0),
      GURL::EmptyGURL(),
      /*is_main_frame=*/true, @"message",
      /*default_text_field_value=*/nil));
  queue->CancelAllRequests();

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbOverlay))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbOverlayJsPrompt))
      << events.back();
}

// Tests logging kBreadcrumbOverlayHttpAuth.
TEST_F(BreadcrumbManagerBrowserAgentTest, HttpAuthOverlay) {
  InsertWebState(browser_.get());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state_list_.GetWebStateAt(0), OverlayModality::kWebContentArea);
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<HTTPAuthOverlayRequestConfig>(
          GURL::EmptyGURL(), "message", "default text"));
  queue->CancelAllRequests();

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbOverlay))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbOverlayHttpAuth))
      << events.back();
}

// Tests logging kBreadcrumbOverlayAppLaunch.
TEST_F(BreadcrumbManagerBrowserAgentTest, AppLaunchOverlay) {
  InsertWebState(browser_.get());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state_list_.GetWebStateAt(0), OverlayModality::kWebContentArea);
  queue->AddRequest(OverlayRequest::CreateWithConfig<
                    app_launcher_overlays::AppLaunchConfirmationRequest>(
      /*is_repeated_request=*/false));
  queue->CancelAllRequests();

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbOverlay))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbOverlayAppLaunch))
      << events.back();
}

// Tests logging kBreadcrumbOverlayAlert with initial and repeated presentation.
TEST_F(BreadcrumbManagerBrowserAgentTest, AlertOverlay) {
  InsertWebState(browser_.get());

  BreadcrumbManagerBrowserAgent::CreateForBrowser(browser_.get());

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      web_state_list_.GetWebStateAt(0), OverlayModality::kWebContentArea);
  // ConfirmDownloadReplacingRequest logged as generic alert.
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<ConfirmDownloadReplacingRequest>());

  std::list<std::string> events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());

  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbOverlay))
      << events.back();
  EXPECT_NE(std::string::npos, events.back().find(kBreadcrumbOverlayAlert))
      << events.back();
  EXPECT_EQ(std::string::npos, events.back().find(kBreadcrumbOverlayActivated))
      << events.back();

  // Switching tabs should log new overlay presentations.
  InsertWebState(browser_.get());
  events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_NE(std::string::npos, events.back().find("Insert active Tab"))
      << events.back();

  web_state_list_.ActivateWebStateAt(0);
  events = breadcrumb_service_->GetEvents(0);
  ASSERT_EQ(4ul, events.size());
  auto activation = std::next(events.begin(), 2);
  EXPECT_NE(std::string::npos, activation->find(kBreadcrumbOverlay))
      << *activation;
  EXPECT_NE(std::string::npos, activation->find(kBreadcrumbOverlayAlert))
      << *activation;
  EXPECT_NE(std::string::npos, activation->find(kBreadcrumbOverlayActivated))
      << *activation;
  queue->CancelAllRequests();
}
