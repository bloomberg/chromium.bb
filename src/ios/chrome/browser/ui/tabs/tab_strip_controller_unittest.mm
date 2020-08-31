// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <memory>

#include "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/popup_menu_commands.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_controller.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_view.h"
#import "ios/chrome/browser/ui/tabs/tab_view.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class TabStripControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    if (!IsIPadIdiom())
      return;

    visible_navigation_item_ = web::NavigationItem::Create();
    mock_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    mock_popup_menu_commands_handler_ =
        OCMStrictProtocolMock(@protocol(PopupMenuCommands));
    mock_application_settings_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationSettingsCommands));

    [browser_.GetCommandDispatcher()
        startDispatchingToTarget:mock_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    [browser_.GetCommandDispatcher()
        startDispatchingToTarget:mock_application_settings_commands_handler_
                     forProtocol:@protocol(ApplicationSettingsCommands)];
    [browser_.GetCommandDispatcher()
        startDispatchingToTarget:mock_popup_menu_commands_handler_
                     forProtocol:@protocol(PopupMenuCommands)];

    controller_ = [[TabStripController alloc] initWithBrowser:&browser_
                                                        style:NORMAL];
  }

  void TearDown() override {
    if (!IsIPadIdiom())
      return;
    [controller_ disconnect];
  }

  void AddWebStateForTesting(std::string title) {
    auto test_web_state = std::make_unique<web::TestWebState>();
    test_web_state->SetTitle(base::UTF8ToUTF16(title));
    auto test_navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    test_navigation_manager->SetVisibleItem(visible_navigation_item_.get());
    test_web_state->SetNavigationManager(std::move(test_navigation_manager));
    browser_.GetWebStateList()->InsertWebState(
        /*index=*/0, std::move(test_web_state), WebStateList::INSERT_NO_FLAGS,
        WebStateOpener());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<web::NavigationItem> visible_navigation_item_;
  TestBrowser browser_;
  id mock_application_commands_handler_;
  id mock_popup_menu_commands_handler_;
  id mock_application_settings_commands_handler_;
  TabStripController* controller_;
  UIWindow* window_;
};

TEST_F(TabStripControllerTest, LoadAndDisplay) {
  if (!IsIPadIdiom())
    return;
  AddWebStateForTesting("Tab Title 1");
  AddWebStateForTesting("Tab Title 2");
  // Force the view to load.
  UIWindow* window = [[UIWindow alloc] initWithFrame:CGRectZero];
  [window addSubview:[controller_ view]];
  window_ = window;

  // There should be two TabViews and one new tab button nested within the
  // parent view (which contains exactly one scroll view).
  EXPECT_EQ(3U,
            [[[[[controller_ view] subviews] objectAtIndex:0] subviews] count]);
}

}  // namespace
