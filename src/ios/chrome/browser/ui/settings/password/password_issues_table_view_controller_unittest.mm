// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues_table_view_controller.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/password/password_issue_with_form.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_presenter.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test class that conforms to PasswordIssuesPresenter in order to test the
// presenter methods are called correctly.
@interface FakePasswordIssuesPresenter : NSObject <PasswordIssuesPresenter>

@property(nonatomic) id<PasswordIssue> presentedPassword;

@end

@implementation FakePasswordIssuesPresenter

- (void)dismissPasswordIssuesTableViewController {
}

- (void)presentPasswordIssueDetails:(id<PasswordIssue>)password {
  _presentedPassword = password;
}

@end

// Unit tests for PasswordIssuesTableViewController.
class PasswordIssuesTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  PasswordIssuesTableViewControllerTest() {
    presenter_ = [[FakePasswordIssuesPresenter alloc] init];
  }

  ChromeTableViewController* InstantiateController() override {
    PasswordIssuesTableViewController* controller =
        [[PasswordIssuesTableViewController alloc]
            initWithStyle:UITableViewStylePlain];
    controller.presenter = presenter_;
    return controller;
  }

  // Adds password issue to the view controller.
  void AddPasswordIssue() {
    auto form = password_manager::PasswordForm();
    form.url = GURL("http://www.example.com/accounts/LoginAuth");
    form.action = GURL("http://www.example.com/accounts/Login");
    form.username_element = u"Email";
    form.username_value = u"test@egmail.com";
    form.password_element = u"Passwd";
    form.password_value = u"test";
    form.submit_element = u"signIn";
    form.signon_realm = "http://www.example.com/";
    form.scheme = password_manager::PasswordForm::Scheme::kHtml;
    NSMutableArray* passwords = [[NSMutableArray alloc] init];
    [passwords
        addObject:[[PasswordIssueWithForm alloc] initWithPasswordForm:form]];

    PasswordIssuesTableViewController* passwords_controller =
        static_cast<PasswordIssuesTableViewController*>(controller());
    [passwords_controller setPasswordIssues:passwords];
  }

  FakePasswordIssuesPresenter* presenter() { return presenter_; }

 private:
  FakePasswordIssuesPresenter* presenter_;
};

// Tests PasswordIssuesViewController is set up with appropriate items
// and sections.
TEST_F(PasswordIssuesTableViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  EXPECT_EQ(1, NumberOfSections());

  EXPECT_EQ(0, NumberOfItemsInSection(0));
}

// Test verifies password issue is displayed correctly.
TEST_F(PasswordIssuesTableViewControllerTest, TestPasswordIssue) {
  CreateController();
  AddPasswordIssue();
  EXPECT_EQ(1, NumberOfSections());

  EXPECT_EQ(1, NumberOfItemsInSection(0));
  CheckTextCellTextAndDetailText(@"example.com", @"test@egmail.com", 0, 0);
}

// Test verifies tapping item triggers function in presenter.
TEST_F(PasswordIssuesTableViewControllerTest, TestPasswordIssueSelection) {
  CreateController();
  AddPasswordIssue();

  PasswordIssuesTableViewController* passwords_controller =
      static_cast<PasswordIssuesTableViewController*>(controller());

  EXPECT_FALSE(presenter().presentedPassword);
  [passwords_controller tableView:passwords_controller.tableView
          didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]];
  EXPECT_TRUE(presenter().presentedPassword);
  EXPECT_NSEQ(@"example.com", presenter().presentedPassword.website);
  EXPECT_NSEQ(@"test@egmail.com", presenter().presentedPassword.username);
}
