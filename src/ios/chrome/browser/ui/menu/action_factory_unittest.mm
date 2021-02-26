// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/menu/action_factory.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/menu/menu_action_type.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/test/ios/ui_image_test_utils.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(__IPHONE_13_0)

namespace {
MenuScenario kTestMenuScenario = MenuScenario::kHistoryEntry;
}  // namespace

// Test fixture for the ActionFactory.
class ActionFactoryTest : public PlatformTest {
 protected:
  ActionFactoryTest()
      : test_title_(@"SomeTitle"),
        test_browser_(std::make_unique<TestBrowser>()) {}

  void SetUp() override {
    mock_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];

    mock_application_settings_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationSettingsCommands));
    [test_browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_settings_commands_handler_
                     forProtocol:@protocol(ApplicationSettingsCommands)];
  }

  // Creates a blue square.
  UIImage* CreateMockImage() {
    return ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
        CGSizeMake(10, 10), [UIColor blueColor]);
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  NSString* test_title_;
  std::unique_ptr<TestBrowser> test_browser_;
  id mock_application_commands_handler_;
  id mock_application_settings_commands_handler_;
};

// Tests the creation of an action using the parameterized method, and verifies
// that the action has the right title and image.
TEST_F(ActionFactoryTest, CreateActionWithParameters) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithBrowser:test_browser_.get()
                                      scenario:kTestMenuScenario];

    UIImage* mockImage = CreateMockImage();

    UIAction* action = [factory actionWithTitle:test_title_
                                          image:mockImage
                                           type:MenuActionType::Copy
                                          block:^{
                                          }];

    EXPECT_TRUE([test_title_ isEqualToString:action.title]);
    EXPECT_EQ(mockImage, action.image);
  }
}

// Tests that the copy action has the right title and image.
TEST_F(ActionFactoryTest, CopyAction) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithBrowser:test_browser_.get()
                                      scenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage imageNamed:@"copy_link_url"];
    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_COPY_LINK_ACTION_TITLE);

    GURL testURL = GURL("https://example.com");

    UIAction* action = [factory actionToCopyURL:testURL];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}

// Tests that the share action has the right title and image.
TEST_F(ActionFactoryTest, ShareAction) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithBrowser:test_browser_.get()
                                      scenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage imageNamed:@"share"];
    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_SHARE_BUTTON_LABEL);

    UIAction* action = [factory actionToShareWithBlock:^{
    }];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}

// Tests that the delete action has the right title and image.
TEST_F(ActionFactoryTest, DeleteAction) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithBrowser:test_browser_.get()
                                      scenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage imageNamed:@"delete"];
    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE);

    UIAction* action = [factory actionToDeleteWithBlock:^{
    }];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
    EXPECT_EQ(UIMenuElementAttributesDestructive, action.attributes);
  }
}

// Tests that the Open in New Tab actions have the right titles and images.
TEST_F(ActionFactoryTest, OpenInNewTabAction_URL) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithBrowser:test_browser_.get()
                                      scenario:kTestMenuScenario];

    GURL testURL = GURL("https://example.com");

    UIImage* expectedImage = [UIImage imageNamed:@"open_in_new_tab"];
    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);

    UIAction* actionWithURL = [factory actionToOpenInNewTabWithURL:testURL
                                                        completion:nil];
    EXPECT_TRUE([expectedTitle isEqualToString:actionWithURL.title]);
    EXPECT_EQ(expectedImage, actionWithURL.image);

    UIAction* actionWithBlock = [factory actionToOpenInNewTabWithBlock:nil];
    EXPECT_TRUE([expectedTitle isEqualToString:actionWithBlock.title]);
    EXPECT_EQ(expectedImage, actionWithBlock.image);
  }
}

// Tests that the Open in New Incognito Tab actions have the right titles
// and images.
TEST_F(ActionFactoryTest, OpenInNewIncognitoTabAction_URL) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithBrowser:test_browser_.get()
                                      scenario:kTestMenuScenario];

    GURL testURL = GURL("https://example.com");

    UIImage* expectedImage = [UIImage imageNamed:@"open_in_incognito"];
    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_OPEN_IN_INCOGNITO_ACTION_TITLE);

    UIAction* actionWithURL =
        [factory actionToOpenInNewIncognitoTabWithURL:testURL completion:nil];
    EXPECT_TRUE([expectedTitle isEqualToString:actionWithURL.title]);
    EXPECT_EQ(expectedImage, actionWithURL.image);

    UIAction* actionWithBlock =
        [factory actionToOpenInNewIncognitoTabWithBlock:nil];
    EXPECT_TRUE([expectedTitle isEqualToString:actionWithBlock.title]);
    EXPECT_EQ(expectedImage, actionWithBlock.image);
  }
}

// Tests that the Open in New Window action has the right title and image.
TEST_F(ActionFactoryTest, OpenInNewWindowAction) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithBrowser:test_browser_.get()
                                      scenario:kTestMenuScenario];

    GURL testURL = GURL("https://example.com");

    UIImage* expectedImage = [UIImage imageNamed:@"open_new_window"];
    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENINNEWWINDOW);

    UIAction* action =
        [factory actionToOpenInNewWindowWithURL:testURL
                                 activityOrigin:WindowActivityToolsOrigin
                                     completion:nil];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}

// Tests that the remove action has the right title and image.
TEST_F(ActionFactoryTest, RemoveAction) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithBrowser:test_browser_.get()
                                      scenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage imageNamed:@"remove"];
    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_REMOVE_ACTION_TITLE);

    UIAction* action = [factory actionToRemoveWithBlock:^{
    }];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}

// Tests that the edit action has the right title and image.
TEST_F(ActionFactoryTest, EditAction) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithBrowser:test_browser_.get()
                                      scenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage imageNamed:@"edit"];
    NSString* expectedTitle = l10n_util::GetNSString(IDS_IOS_EDIT_ACTION_TITLE);

    UIAction* action = [factory actionToEditWithBlock:^{
    }];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}

// Tests that the Open All Tabs action has the right title and image.
TEST_F(ActionFactoryTest, openAllTabsAction) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithBrowser:test_browser_.get()
                                      scenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage systemImageNamed:@"plus"];
    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPEN_ALL_LINKS);

    UIAction* action = [factory actionToOpenAllTabsWithBlock:^{
    }];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}

// Tests that the hide action has the right title and image.
TEST_F(ActionFactoryTest, hideAction) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithBrowser:test_browser_.get()
                                      scenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage imageNamed:@"remove"];
    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_RECENT_TABS_HIDE_MENU_OPTION);

    UIAction* action = [factory actionToHideWithBlock:^{
    }];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}

// Tests that the Move Folder action has the right title and image.
TEST_F(ActionFactoryTest, MoveFolderAction) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithBrowser:test_browser_.get()
                                      scenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage imageNamed:@"move_folder"];

    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE);

    UIAction* action = [factory actionToMoveFolderWithBlock:^{
    }];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}

// Tests that the Mark As Read action has the right title and image.
TEST_F(ActionFactoryTest, markAsReadAction) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithBrowser:test_browser_.get()
                                      scenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage imageNamed:@"mark_read"];

    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_READING_LIST_MARK_AS_READ_ACTION);

    UIAction* action = [factory actionToMarkAsReadWithBlock:^{
    }];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}

// Tests that the Mark As Unread action has the right title and image.
TEST_F(ActionFactoryTest, markAsUnreadAction) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithBrowser:test_browser_.get()
                                      scenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage imageNamed:@"remove"];

    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_READING_LIST_MARK_AS_UNREAD_ACTION);

    UIAction* action = [factory actionToMarkAsUnreadWithBlock:^{
    }];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}

// Tests that the View Offline Version in New Tab action has the right title and
// image.
TEST_F(ActionFactoryTest, viewOfflineVersion) {
  if (@available(iOS 13.0, *)) {
    ActionFactory* factory =
        [[ActionFactory alloc] initWithBrowser:test_browser_.get()
                                      scenario:kTestMenuScenario];

    UIImage* expectedImage = [UIImage imageNamed:@"offline"];

    NSString* expectedTitle =
        l10n_util::GetNSString(IDS_IOS_READING_LIST_OPEN_OFFLINE_BUTTON);

    UIAction* action = [factory actionToOpenOfflineVersionInNewTabWithBlock:^{
    }];

    EXPECT_TRUE([expectedTitle isEqualToString:action.title]);
    EXPECT_EQ(expectedImage, action.image);
  }
}

#endif  // defined(__IPHONE_13_0)
