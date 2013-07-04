// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/signin_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#import "testing/gtest_mac.h"
#include "ui/app_list/signin_delegate.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

@class TestSigninViewDelegate;

namespace {

// Helper function to cycle through the responder chain created implicitly
// from subviews, without having to interact with the event system.
NSControl* NextControl(NSControl* control) {
  NSArray* siblings = [[control superview] subviews];
  for (NSUInteger index = [siblings indexOfObject:control] + 1;
       index < [siblings count] ; ++index) {
    if ([[siblings objectAtIndex:index] acceptsFirstResponder])
      return [siblings objectAtIndex:index];
  }
  return nil;
}

}  // namespace

namespace app_list {
namespace test {

class SigninViewControllerTest : public ui::CocoaTest,
                                 public SigninDelegate {
 public:
  SigninViewControllerTest()
      : test_text_(ASCIIToUTF16("Sign in")),
        needs_signin_(true),
        show_signin_count_(0),
        open_learn_more_count_(0),
        open_settings_count_(0) {}

  // ui::CocoaTest override:
  virtual void SetUp() OVERRIDE;

  // SigninDelegate overrides:
  virtual bool NeedSignin() OVERRIDE { return needs_signin_; }
  virtual void ShowSignin() OVERRIDE { ++show_signin_count_; }
  virtual void OpenLearnMore() OVERRIDE { ++open_learn_more_count_; }
  virtual void OpenSettings() OVERRIDE { ++open_settings_count_; }

  virtual base::string16 GetSigninHeading() OVERRIDE { return test_text_; }
  virtual base::string16 GetSigninText() OVERRIDE { return test_text_; }
  virtual base::string16 GetSigninButtonText() OVERRIDE { return test_text_; }
  virtual base::string16 GetLearnMoreLinkText() OVERRIDE { return test_text_; }
  virtual base::string16 GetSettingsLinkText() OVERRIDE { return test_text_; }

 protected:
  const base::string16 test_text_;
  base::scoped_nsobject<SigninViewController> signin_view_controller_;
  bool needs_signin_;
  int show_signin_count_;
  int open_learn_more_count_;
  int open_settings_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninViewControllerTest);
};

void SigninViewControllerTest::SetUp() {
  NSRect frame = NSMakeRect(0, 0, 400, 500);
  signin_view_controller_.reset(
      [[SigninViewController alloc] initWithFrame:frame
                                     cornerRadius:3
                                         delegate:this]);

  ui::CocoaTest::SetUp();
  [[test_window() contentView] addSubview:[signin_view_controller_ view]];
}

TEST_VIEW(SigninViewControllerTest, [signin_view_controller_ view]);

TEST_F(SigninViewControllerTest, NotSignedIn) {
  NSArray* content_subviews = [[test_window() contentView] subviews];
  EXPECT_EQ(1u, [content_subviews count]);
  NSArray* subviews = [[content_subviews objectAtIndex:0] subviews];
  EXPECT_LT(0u, [subviews count]);

  // The first subview that acceptFirstResponder should be the signin button,
  // and performing its action should show the signin dialog. Then "Learn more",
  // followed by "Settings".
  NSControl* control = NextControl([subviews objectAtIndex:0]);
  EXPECT_EQ(0, show_signin_count_);
  EXPECT_TRUE([[control target] performSelector:[control action]
                                     withObject:control]);
  EXPECT_EQ(1, show_signin_count_);

  control = NextControl(control);
  EXPECT_EQ(0, open_learn_more_count_);
  EXPECT_TRUE([[control target] performSelector:[control action]
                                     withObject:control]);
  EXPECT_EQ(1, open_learn_more_count_);

  control = NextControl(control);
  EXPECT_EQ(0, open_settings_count_);
  EXPECT_TRUE([[control target] performSelector:[control action]
                                     withObject:control]);
  EXPECT_EQ(1, open_settings_count_);
}

}  // namespace test
}  // namespace app_list
