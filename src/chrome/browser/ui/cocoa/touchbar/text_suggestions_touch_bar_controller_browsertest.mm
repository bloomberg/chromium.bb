// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/touchbar/text_suggestions_touch_bar_controller.h"
#include "chrome/browser/ui/cocoa/touchbar/web_textfield_touch_bar_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/base/cocoa/touch_bar_util.h"

API_AVAILABLE(macos(10.12.2))
@interface MockWebTextfieldTouchBarController : WebTextfieldTouchBarController {
  // Counter for the number of times invalidateTouchBar is called.
  int numInvalidations_;
}
- (int)numInvalidations;

- (void)resetNumInvalidations;

@end

@implementation MockWebTextfieldTouchBarController

- (void)invalidateTouchBar {
  numInvalidations_++;
}

- (int)numInvalidations {
  return numInvalidations_;
}

- (void)resetNumInvalidations {
  numInvalidations_ = 0;
}

@end

API_AVAILABLE(macos(10.12.2))
@interface MockTextSuggestionsTouchBarController
    : TextSuggestionsTouchBarController
- (NSString*)firstSuggestion;
@end

@implementation MockTextSuggestionsTouchBarController

- (void)requestSuggestions {
  [self setSuggestions:@[ [self text] ]];
  [[self controller] invalidateTouchBar];
}

- (NSString*)firstSuggestion {
  return [self suggestions][0];
}

@end

namespace {

class TextSuggestionsTouchBarControllerTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    InProcessBrowserTest::SetUp();
    feature_list_.InitAndEnableFeature(features::kTextSuggestionsTouchBar);
  }

  void SetUpOnMainThread() override {
    if (@available(macOS 10.12.2, *)) {
      web_textfield_controller_.reset(
          [[MockWebTextfieldTouchBarController alloc] init]);
      [web_textfield_controller_ resetNumInvalidations];
      touch_bar_controller_.reset([[MockTextSuggestionsTouchBarController alloc]
          initWithWebContents:GetActiveWebContents()
                   controller:web_textfield_controller_]);
    }
  }

  void FocusTextfield() {
    content::WindowedNotificationObserver focus_observer(
        content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
        content::NotificationService::AllSources());
    ui_test_utils::NavigateToURL(
        browser(),
        GURL("data:text/html;charset=utf-8,<input type=\"text\" autofocus>"));
    focus_observer.Wait();
  }

  void UnfocusTextfield() {
    ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  API_AVAILABLE(macos(10.12.2))
  base::scoped_nsobject<MockWebTextfieldTouchBarController>
      web_textfield_controller_;

  API_AVAILABLE(macos(10.12.2))
  base::scoped_nsobject<MockTextSuggestionsTouchBarController>
      touch_bar_controller_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests to check if the touch bar shows up properly.
IN_PROC_BROWSER_TEST_F(TextSuggestionsTouchBarControllerTest, MakeTouchBar) {
  if (@available(macOS 10.12.2, *)) {
    NSString* const kTextSuggestionsTouchBarId = @"text-suggestions";
    NSArray* const kSuggestions = @[ @"text" ];

    // Touch bar shouldn't appear if the focused element is not a textfield.
    UnfocusTextfield();
    [touch_bar_controller_ setSuggestions:kSuggestions];
    EXPECT_FALSE([touch_bar_controller_ makeTouchBar]);

    // Touch bar shouldn't appear if there are no suggestions.
    FocusTextfield();
    [touch_bar_controller_ setSuggestions:[NSArray array]];
    EXPECT_FALSE([touch_bar_controller_ makeTouchBar]);

    // Touch bar should appear if textfield is focused and there are
    // suggestions.
    [touch_bar_controller_ setSuggestions:kSuggestions];
    NSTouchBar* touch_bar = [touch_bar_controller_ makeTouchBar];
    EXPECT_TRUE(touch_bar);
    EXPECT_TRUE([[touch_bar customizationIdentifier]
        isEqual:ui::GetTouchBarId(kTextSuggestionsTouchBarId)]);
  }
}

// Tests that a change in text selection is handled properly.
IN_PROC_BROWSER_TEST_F(TextSuggestionsTouchBarControllerTest,
                       UpdateTextSelection) {
  if (@available(macOS 10.12.2, *)) {
    NSString* const kText = @"text";
    NSString* const kEmptyText = @"";
    const gfx::Range kRange = gfx::Range(0, 4);
    const gfx::Range kEmptyRange = gfx::Range();

    // If not in a textfield,
    //  1. Textfield text should not be saved.
    //  2. Selected range should not be saved.
    //  3. Touch bar should be invalidated (if on MacOS 10.12.2 or later).
    UnfocusTextfield();
    [touch_bar_controller_ setText:kEmptyText];
    [touch_bar_controller_ setSelectionRange:kEmptyRange];
    [web_textfield_controller_ resetNumInvalidations];

    [touch_bar_controller_ updateTextSelection:base::SysNSStringToUTF16(kText)
                                         range:kRange
                                        offset:0];
    EXPECT_STREQ(kEmptyText.UTF8String,
                 [touch_bar_controller_ text].UTF8String);
    EXPECT_EQ(kEmptyRange, [touch_bar_controller_ selectionRange]);
    EXPECT_EQ(1, [web_textfield_controller_ numInvalidations]);

    // If in a textfield and on MacOS 10.12.2 or later,
    //   1. Textfield text should be saved.
    //   2. Selected range should be saved.
    //   3. Suggestions should be generated based on text selection.
    //   4. Touch bar should be invalidated.
    FocusTextfield();
    [touch_bar_controller_ setText:kEmptyText];
    [touch_bar_controller_ setSelectionRange:kEmptyRange];
    [web_textfield_controller_ resetNumInvalidations];

    [touch_bar_controller_ updateTextSelection:base::SysNSStringToUTF16(kText)
                                         range:kRange
                                        offset:0];
    EXPECT_STREQ(kText.UTF8String, [touch_bar_controller_ text].UTF8String);
    EXPECT_EQ(kRange, [touch_bar_controller_ selectionRange]);
    EXPECT_STREQ(kText.UTF8String,
                 [touch_bar_controller_ firstSuggestion].UTF8String);
    EXPECT_EQ(1, [web_textfield_controller_ numInvalidations]);
  }
}

// Tests that a range outside of the text bounds will set the selection range
// to empty.
IN_PROC_BROWSER_TEST_F(TextSuggestionsTouchBarControllerTest,
                       RangeOutOfTextBounds) {
  if (@available(macOS 10.12.2, *)) {
    FocusTextfield();
    [touch_bar_controller_ setText:@""];
    [touch_bar_controller_ setSelectionRange:gfx::Range()];

    [touch_bar_controller_
        updateTextSelection:base::string16(base::ASCIIToUTF16("text"))
                      range:gfx::Range(4, 5)
                     offset:0];
    EXPECT_STREQ("", [touch_bar_controller_ text].UTF8String);
    EXPECT_EQ(gfx::Range(), [touch_bar_controller_ selectionRange]);

    [touch_bar_controller_
        updateTextSelection:base::string16(base::ASCIIToUTF16("text"))
                      range:gfx::Range(2, 5)
                     offset:0];
    EXPECT_STREQ("", [touch_bar_controller_ text].UTF8String);
    EXPECT_EQ(gfx::Range(), [touch_bar_controller_ selectionRange]);
  }
}

// Tests that a change in WebContents is handled properly.
IN_PROC_BROWSER_TEST_F(TextSuggestionsTouchBarControllerTest, SetWebContents) {
  if (@available(macOS 10.12.2, *)) {
    NSString* const kText = @"text";
    const gfx::Range kRange = gfx::Range(1, 1);

    FocusTextfield();

    // A null-pointer should not break the controller.
    [touch_bar_controller_ setWebContents:nullptr];
    EXPECT_FALSE([touch_bar_controller_ webContents]);

    [touch_bar_controller_ setText:kText];
    [touch_bar_controller_ setSelectionRange:kRange];

    // The text selection should change on MacOS 10.12.2 and later if the
    // WebContents pointer is not null.
    [touch_bar_controller_ setWebContents:GetActiveWebContents()];
    EXPECT_EQ(GetActiveWebContents(), [touch_bar_controller_ webContents]);
    EXPECT_STRNE(kText.UTF8String, [touch_bar_controller_ text].UTF8String);
    EXPECT_NE(kRange, [touch_bar_controller_ selectionRange]);
  }
}

// Tests that the selection created when replacing the editing word with a
// suggestion is ignored.
IN_PROC_BROWSER_TEST_F(TextSuggestionsTouchBarControllerTest,
                       IgnoreReplacementSelection) {
  if (@available(macOS 10.12.2, *)) {
    NSString* const kText = @"text";
    const base::string16 kEmptyText = base::string16();
    const gfx::Range kRange = gfx::Range(0, 4);
    const gfx::Range kEmptyRange = gfx::Range();

    FocusTextfield();
    [touch_bar_controller_ setText:kText];
    [touch_bar_controller_ setSelectionRange:kRange];

    // If ignoreReplacementSelection is YES and new selection range is equal to
    // editing word range, ignore text selection update.
    [touch_bar_controller_ setShouldIgnoreReplacementSelection:YES];
    [touch_bar_controller_ setEditingWordRange:kEmptyRange offset:0];
    [touch_bar_controller_ updateTextSelection:kEmptyText
                                         range:kEmptyRange
                                        offset:0];
    EXPECT_STREQ(kText.UTF8String, [touch_bar_controller_ text].UTF8String);
    EXPECT_EQ(kRange, [touch_bar_controller_ selectionRange]);

    // If ignoreReplacementSelection is YES but new selection range is not equal
    // to editing word range, do not ignore text selection update.
    [touch_bar_controller_ setShouldIgnoreReplacementSelection:YES];
    [touch_bar_controller_ setEditingWordRange:kRange offset:0];
    [touch_bar_controller_ updateTextSelection:kEmptyText
                                         range:kEmptyRange
                                        offset:0];
    EXPECT_STREQ("", [touch_bar_controller_ text].UTF8String);
    EXPECT_EQ(gfx::Range(), [touch_bar_controller_ selectionRange]);

    [touch_bar_controller_ setText:kText];
    [touch_bar_controller_ setSelectionRange:kRange];

    // If ignoreReplacementSelection is NO and new selection range is equal to
    // editing word range, do not ignore text selection update.
    [touch_bar_controller_ setShouldIgnoreReplacementSelection:NO];
    [touch_bar_controller_ setEditingWordRange:kEmptyRange offset:0];
    [touch_bar_controller_ updateTextSelection:kEmptyText
                                         range:kEmptyRange
                                        offset:0];
    EXPECT_STREQ("", [touch_bar_controller_ text].UTF8String);
    EXPECT_EQ(gfx::Range(), [touch_bar_controller_ selectionRange]);
  }
}

// Tests that offsets are properly handled.
IN_PROC_BROWSER_TEST_F(TextSuggestionsTouchBarControllerTest, Offset) {
  if (@available(macOS 10.12.2, *)) {
    const base::string16 kText =
        base::string16(base::ASCIIToUTF16("hello world"));
    const gfx::Range kRange = gfx::Range(3, 3);
    const size_t kOffset = 1;
    const gfx::Range kOffsetRange =
        gfx::Range(kRange.start() - kOffset, kRange.end() - kOffset);

    FocusTextfield();
    [touch_bar_controller_ setSelectionRange:gfx::Range()];

    // |selectionRange_| should include offset.
    [touch_bar_controller_ updateTextSelection:kText
                                         range:kRange
                                        offset:kOffset];
    if (@available(macOS 10.12.2, *))
      EXPECT_EQ(kOffsetRange, [touch_bar_controller_ selectionRange]);
    else
      EXPECT_EQ(gfx::Range(), [touch_bar_controller_ selectionRange]);

    // The check for ignoring text selection updates should still work with
    // offsets.
    [touch_bar_controller_ setShouldIgnoreReplacementSelection:YES];
    [touch_bar_controller_ updateTextSelection:kText
                                         range:gfx::Range(1, 7)
                                        offset:kOffset];
    if (@available(macOS 10.12.2, *))
      EXPECT_EQ(gfx::Range(0, 6), [touch_bar_controller_ selectionRange]);
    else
      EXPECT_EQ(gfx::Range(), [touch_bar_controller_ selectionRange]);
  }
}

}  // namespace
