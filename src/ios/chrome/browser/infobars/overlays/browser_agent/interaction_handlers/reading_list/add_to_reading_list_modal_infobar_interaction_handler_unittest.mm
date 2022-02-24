// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/reading_list/add_to_reading_list_modal_infobar_interaction_handler.h"

#include "base/test/task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_ios_add_to_reading_list_infobar_delegate.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#include "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/reading_list/fake_reading_list_model.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for ReadingListInfobarModalInteractionHandler.
class ReadingListInfobarModalInteractionHandlerTest : public PlatformTest {
 public:
  ReadingListInfobarModalInteractionHandlerTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    mock_command_receiver_ = OCMStrictProtocolMock(@protocol(BrowserCommands));
    handler_ = std::make_unique<ReadingListInfobarModalInteractionHandler>(
        browser_.get());
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfobarOverlayRequestInserter::CreateForWebState(&web_state_);
    InfoBarManagerImpl::CreateForWebState(&web_state_);
    fake_reading_list_model_ = std::make_unique<FakeReadingListModel>();

    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_command_receiver_
                     forProtocol:@protocol(BrowserCommands)];

    std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeAddToReadingList,
        std::make_unique<MockIOSAddToReadingListInfobarDelegate>(
            fake_reading_list_model_.get(), &web_state_));
    infobar_ = infobar.get();
    InfoBarManagerImpl::FromWebState(&web_state_)
        ->AddInfoBar(std::move(infobar));
  }

  void TearDown() override {
    [browser_->GetCommandDispatcher()
        stopDispatchingToTarget:mock_command_receiver_];
    EXPECT_OCMOCK_VERIFY(mock_command_receiver_);
    PlatformTest::TearDown();
  }

  MockIOSAddToReadingListInfobarDelegate& mock_delegate() {
    return *static_cast<MockIOSAddToReadingListInfobarDelegate*>(
        infobar_->delegate());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  id mock_command_receiver_ = nil;
  std::unique_ptr<ReadingListInfobarModalInteractionHandler> handler_;
  web::FakeWebState web_state_;
  std::unique_ptr<FakeReadingListModel> fake_reading_list_model_;
  InfoBarIOS* infobar_;
};

// Tests MainButtonTapped() calls Accept() on the mock delegate and resets
// the infobar to be accepted.
TEST_F(ReadingListInfobarModalInteractionHandlerTest, MainButton) {
  ASSERT_FALSE(infobar_->accepted());
  EXPECT_CALL(mock_delegate(), Accept()).WillOnce(testing::Return(true));
  OCMExpect([mock_command_receiver_ showReadingListIPH]);
  handler_->PerformMainAction(infobar_);
  EXPECT_TRUE(infobar_->accepted());
}

// Tests NeverAsk() calls proper delegate methods.
TEST_F(ReadingListInfobarModalInteractionHandlerTest, NeverAsk) {
  EXPECT_CALL(mock_delegate(), NeverShow()).Times(1);
  handler_->NeverAsk(infobar_);
}
