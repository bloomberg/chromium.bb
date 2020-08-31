// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#include "build/build_config.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/table_view/feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::BookmarksDeleteSwipeButton;
using chrome_test_util::BookmarksNavigationBarBackButton;
using chrome_test_util::BookmarksSaveEditFolderButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ContextBarCenterButtonWithLabel;
using chrome_test_util::ContextBarLeadingButtonWithLabel;
using chrome_test_util::ContextMenuCopyButton;
using chrome_test_util::OmniboxText;
using chrome_test_util::TappableBookmarkNodeWithLabel;

// Bookmark entries integration tests for Chrome.
@interface BookmarksEntriesTestCase : ChromeTestCase
@end

@implementation BookmarksEntriesTestCase

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  [ChromeEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];
}

#pragma mark - BookmarksEntriesTestCase Tests

- (void)testUndoDeleteBookmarkFromSwipe {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1035764): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif
  // TODO(crbug.com/851227): On Compact Width, the bookmark cell is being
  // deleted by grey_swipeFastInDirection.
  // grey_swipeFastInDirectionWithStartPoint doesn't work either and it might
  // fail on devices. Disabling this test under these conditions on the
  // meantime.
  if (![ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iPad.");
  }

  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Swipe action on the URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify context bar does not change when "Delete" shows up.
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];
  // Delete it.
  [[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarkEarlGreyUI waitForDeletionOfBookmarkWithTitle:@"Second URL"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notNil()];

  // Verify context bar remains in default state.
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];
}

// TODO(crbug.com/781445): Re-enable this test on 32-bit.
#if defined(ARCH_CPU_64_BITS)
#define MAYBE_testSwipeToDeleteDisabledInEditMode \
  testSwipeToDeleteDisabledInEditMode
#else
#define MAYBE_testSwipeToDeleteDisabledInEditMode \
  FLAKY_testSwipeToDeleteDisabledInEditMode
#endif
- (void)testSwipeToDeleteDisabledInEditMode {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1035764): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif
  // TODO(crbug.com/851227): On non Compact Width  the bookmark cell is being
  // deleted by grey_swipeFastInDirection.
  // grey_swipeFastInDirectionWithStartPoint doesn't work either and it might
  // fail on devices. Disabling this test under these conditions on the
  // meantime.
  if (![ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iPad on iOS11.");
  }

  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Swipe action on the URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify the delete confirmation button shows up.
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClassName(@"UITableView")]
      assertWithMatcher:grey_notNil()];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Verify the delete confirmation button is gone after entering edit mode.
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClassName(@"UITableView")]
      assertWithMatcher:grey_nil()];

  // Swipe action on "Second URL".  This should not bring out delete
  // confirmation button as swipe-to-delete is disabled in edit mode.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify the delete confirmation button doesn't appear.
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClassName(@"UITableView")]
      assertWithMatcher:grey_nil()];

  // Cancel edit mode
  [BookmarkEarlGreyUI closeContextBarEditMode];

  // Swipe action on the URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"French URL")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify the delete confirmation button shows up. (swipe-to-delete is
  // re-enabled).
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClassName(@"UITableView")]
      assertWithMatcher:grey_notNil()];
}

- (void)testContextMenuForSingleURLSelection {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1035764): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  [BookmarkEarlGreyUI verifyContextMenuForSingleURLWithEditEnabled:YES];
}

// Verify Edit Text functionality on single URL selection.
- (void)testEditTextOnSingleURL {
  // TODO(crbug.com/1049972): Re-enable on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }

  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // 1. Edit the bookmark title at edit page.

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_tap()];

  // Modify the title.
  [BookmarkEarlGreyUI
      tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT
                  openEditor:kBookmarkEditViewContainerIdentifier
             modifyTextField:@"Title Field_textField"
                          to:@"n5"
                 dismissWith:kBookmarkEditNavigationBarDoneButtonIdentifier];

  // Verify that the bookmark was updated.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"n5")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Press undo and verify old URL is back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];

  // 2. Edit the bookmark url at edit page.

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];

  // Modify the url.
  [BookmarkEarlGreyUI
      tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT
                  openEditor:kBookmarkEditViewContainerIdentifier
             modifyTextField:@"URL Field_textField"
                          to:@"www.b.fr"
                 dismissWith:kBookmarkEditNavigationBarDoneButtonIdentifier];

  // Verify that the bookmark was updated.
  [BookmarkEarlGrey verifyExistenceOfBookmarkWithURL:@"http://www.b.fr/"
                                                name:@"French URL"];

  // Press undo and verify the edit is undone.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];
  [BookmarkEarlGrey verifyAbsenceOfBookmarkWithURL:@"http://www.b.fr/"];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];
}

// Verify Move functionality on single URL selection.
- (void)testMoveOnSingleURL {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1035764): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // 1. Move a single url at edit page.

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select single url.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Move the "Second URL" to "Folder 1.1".
  [BookmarkEarlGreyUI
      tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT
                  openEditor:kBookmarkEditViewContainerIdentifier
           setParentFolderTo:@"Folder 1.1"
                        from:@"Mobile Bookmarks"];

  // Verify edit mode remains.
  [BookmarkEarlGreyUI verifyContextBarInEditMode];

  // Close edit mode.
  [BookmarkEarlGreyUI closeContextBarEditMode];

  // Navigate to "Folder 1.1" and verify "Second URL" is under it.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // 2. Test the cancel button at edit page.

  // Come back to the Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:BookmarksNavigationBarBackButton()]
      performAction:grey_tap()];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];

  // Tap cancel after modifying the url.
  [BookmarkEarlGreyUI
      tapOnContextMenuButton:IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT
                  openEditor:kBookmarkEditViewContainerIdentifier
             modifyTextField:@"URL Field_textField"
                          to:@"www.b.fr"
                 dismissWith:@"Cancel"];

  // Verify that the bookmark was not updated.
  [BookmarkEarlGrey verifyAbsenceOfBookmarkWithURL:@"http://www.b.fr/"];

  // Verify edit mode remains.
  [BookmarkEarlGreyUI verifyContextBarInEditMode];
}

// Verify Copy URL functionality on single URL selection.
- (void)testCopyFunctionalityOnSingleURL {
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];

  // Invoke Copy through context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Select Copy URL.
  [[EarlGrey selectElementWithMatcher:ContextMenuCopyButton()]
      performAction:grey_tap()];

  // Verify general pasteboard has the URL copied.
  ConditionBlock condition = ^{
    return !![[UIPasteboard generalPasteboard].string
        containsString:@"www.a.fr"];
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(10, condition),
             @"Waiting for URL to be copied to pasteboard.");

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];
}

- (void)testContextMenuForMultipleURLSelection {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1035764): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URLs.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Verify it shows the context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeContextMenuIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN_INCOGNITO)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify the Open All functionality on multiple url selection.
// TODO(crbug.com/816699): Re-enable this test on simulators.
- (void)FLAKY_testContextMenuForMultipleURLOpenAll {
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Open 3 normal tabs from a normal session.
  [BookmarkEarlGreyUI selectUrlsAndTapOnContextBarButtonWithLabelId:
                          IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN];

  // Verify there are 3 normal tabs.
  [ChromeEarlGrey waitForMainTabCount:3];
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");

  // Verify the order of open tabs.
  [BookmarksEntriesTestCase verifyOrderOfTabsWithCurrentTabIndex:0];

  // Switch to Incognito mode by adding a new incognito tab.
  [ChromeEarlGrey openNewIncognitoTab];

  [BookmarkEarlGreyUI openBookmarks];

  // Open 3 normal tabs from a incognito session.
  [BookmarkEarlGreyUI selectUrlsAndTapOnContextBarButtonWithLabelId:
                          IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN];

  // Verify there are 6 normal tabs and no new incognito tabs.
  [ChromeEarlGrey waitForMainTabCount:6];
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 1,
                 @"Incognito tab count should be 1");

  // Close the incognito tab to go back to normal mode.
  [ChromeEarlGrey closeAllIncognitoTabs];

  // The following verifies the selected bookmarks are open in the same order as
  // in folder.

  // Verify the order of open tabs.
  [BookmarksEntriesTestCase verifyOrderOfTabsWithCurrentTabIndex:3];
}

// Verify the Open All in Incognito functionality on multiple url selection.
// TODO(crbug.com/816699): Re-enable this test on simulators.
- (void)FLAKY_testContextMenuForMultipleURLOpenAllInIncognito {
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Open 3 incognito tabs from a normal session.
  [BookmarkEarlGreyUI selectUrlsAndTapOnContextBarButtonWithLabelId:
                          IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN_INCOGNITO];

  // Verify there are 3 incognito tabs and no new normal tab.
  [ChromeEarlGrey waitForIncognitoTabCount:3];
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");

  // Verify the current tab is an incognito tab.
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Failed to switch to incognito mode");

  // Verify the order of open tabs.
  [BookmarksEntriesTestCase verifyOrderOfTabsWithCurrentTabIndex:0];

  [BookmarkEarlGreyUI openBookmarks];

  // Open 3 incognito tabs from a incognito session.
  [BookmarkEarlGreyUI selectUrlsAndTapOnContextBarButtonWithLabelId:
                          IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN_INCOGNITO];

  // The 3rd tab will be re-used to open one of the selected bookmarks.  So
  // there will be 2 new tabs only.

  // Verify there are 5 incognito tabs and no new normal tab.
  [ChromeEarlGrey waitForIncognitoTabCount:5];
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 1,
                 @"Main tab count should be 1");

  // Verify the order of open tabs.
  [BookmarksEntriesTestCase verifyOrderOfTabsWithCurrentTabIndex:2];
}

// Verify the Open and Open in Incognito functionality on single url.
- (void)testOpenSingleBookmarkInNormalAndIncognitoTab {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1035764): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Open a bookmark in current tab in a normal session.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];

  // Verify "First URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFirstUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarkEarlGreyUI openBookmarks];

  // Open a bookmark in new tab from a normal session (through a long press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      performAction:grey_tap()];

  // Verify there is 1 new normal tab created and no new incognito tab created.
  [ChromeEarlGrey waitForMainTabCount:2];
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 0,
                 @"Incognito tab count should be 0");

  // Verify "Second URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetSecondUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarkEarlGreyUI openBookmarks];

  // Open a bookmark in an incognito tab from a normal session (through a long
  // press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
      performAction:grey_tap()];

  // Verify there is 1 incognito tab created and no new normal tab created.
  [ChromeEarlGrey waitForIncognitoTabCount:1];
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");

  // Verify the current tab is an incognito tab.
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Failed to switch to incognito mode");

  // Verify "French URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFrenchUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarkEarlGreyUI openBookmarks];

  // Open a bookmark in current tab from a incognito session.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];

  // Verify "First URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFirstUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  // Verify the current tab is an incognito tab.
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Failed to staying at incognito mode");

  // Verify no new tabs created.
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 1,
                 @"Incognito tab count should be 1");
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");

  [BookmarkEarlGreyUI openBookmarks];

  // Open a bookmark in new incognito tab from a incognito session (through a
  // long press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
      performAction:grey_tap()];

  // Verify a new incognito tab is created.
  [ChromeEarlGrey waitForIncognitoTabCount:2];
  GREYAssertTrue([ChromeEarlGrey mainTabCount] == 2,
                 @"Main tab count should be 2");

  // Verify the current tab is an incognito tab.
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Failed to staying at incognito mode");

  // Verify "Second URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetSecondUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarkEarlGreyUI openBookmarks];

  // Open a bookmark in a new normal tab from a incognito session (through a
  // long press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      performAction:grey_tap()];

  // Verify a new normal tab is created and no incognito tab is created.
  [ChromeEarlGrey waitForMainTabCount:3];
  GREYAssertTrue([ChromeEarlGrey incognitoTabCount] == 2,
                 @"Incognito tab count should be 2");

  // Verify the current tab is a normal tab.
  GREYAssertFalse([ChromeEarlGrey isIncognitoMode],
                  @"Failed to switch to normal mode");

  // Verify "French URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFrenchUrl().GetContent())]
      assertWithMatcher:grey_notNil()];
}

- (void)testContextMenuForMixedSelection {
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL and folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Verify it shows the context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeContextMenuIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testLongPressOnSingleURL {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1035764): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_longPress()];

  // Verify context menu.
  [BookmarkEarlGreyUI verifyContextMenuForSingleURLWithEditEnabled:YES];
}

// Verify Move functionality on mixed folder / url selection.
- (void)testMoveFunctionalityOnMixedSelection {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1035764): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL and folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Tap on move, from context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Verify folder picker appeared.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Delete the First URL programmatically in background.  Folder picker will
  // not close as the selected nodes "Second URL" and "Folder 1" still exist.
  [BookmarkEarlGrey removeBookmarkWithTitle:@"First URL"];

  // Choose to move into a new folder.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkCreateNewFolderCellIdentifier)]
      performAction:grey_tap()];

  // Enter custom new folder name.
  [BookmarkEarlGreyUI
      renameBookmarkFolderWithFolderTitle:@"Title For New Folder"];

  // Verify current parent folder for "Title For New Folder" folder is "Mobile
  // Bookmarks" folder.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Mobile Bookmarks"),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done to close bookmark move flow.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
  [BookmarkEarlGreyUI verifyFolderFlowIsClosed];

  // Wait for Undo toast to go away from screen.
  [BookmarkEarlGreyUI waitForUndoToastToGoAway];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];

  // Verify new folder "Title For New Folder" has two bookmark nodes.
  [BookmarkEarlGrey verifyChildCount:2
                    inFolderWithName:@"Title For New Folder"];

  // Drill down to where "Second URL" and "Folder 1" have been moved and assert
  // it's presence.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Title For New Folder")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify Move functionality on multiple url selection.
- (void)testMoveFunctionalityOnMultipleUrlSelection {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1035764): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL and folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Tap on move, from context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Choose to move into Folder 1. Use grey_ancestor since
  // BookmarksHomeTableView might be visible on the background on non-compact
  // widthts, and there might be a "Folder1" node there as well.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(TappableBookmarkNodeWithLabel(@"Folder 1"),
                            grey_ancestor(grey_accessibilityID(
                                kBookmarkFolderPickerViewContainerIdentifier)),
                            nil)] performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
  [BookmarkEarlGreyUI verifyFolderFlowIsClosed];

  // Wait for Undo toast to go away from screen.
  [BookmarkEarlGreyUI waitForUndoToastToGoAway];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];

  // Verify Folder 1 has three bookmark nodes.
  [BookmarkEarlGrey verifyChildCount:3 inFolderWithName:@"Folder 1"];

  // Drill down to where "Second URL" and "First URL" have been moved and assert
  // it's presence.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify Move is cancelled when all selected folder/url are deleted in
// background.
- (void)testMoveCancelledWhenAllSelectionDeleted {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1035764): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode, using context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URL and folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Tap on move, from context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Verify folder picker appeared.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Delete the selected URL and folder programmatically.
  [BookmarkEarlGrey removeBookmarkWithTitle:@"Folder 1"];
  [BookmarkEarlGrey removeBookmarkWithTitle:@"Second URL"];

  // Verify folder picker is exited.
  [BookmarkEarlGreyUI verifyFolderFlowIsClosed];
}

// Try deleting a bookmark from the edit screen, then undoing that delete.
- (void)testUndoDeleteBookmarkFromEditScreen {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1035764): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select Folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Tap Edit Folder.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditorDeleteButtonIdentifier)]
      performAction:grey_tap()];

  // Wait until it's gone.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(10, condition),
             @"Waiting for bookmark to go away");

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_notNil()];

  // Verify Delete is disabled (with visible Delete, it also means edit mode is
  // stayed).
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
}

- (void)testDeleteSingleURLNode {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1035764): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select single URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarkEarlGreyUI waitForDeletionOfBookmarkWithTitle:@"Second URL"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notNil()];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];
}

- (void)testDeleteMultipleNodes {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1035764): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select Folder and URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey
      selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarkEarlGreyUI waitForDeletionOfBookmarkWithTitle:@"Second URL"];
  [BookmarkEarlGreyUI waitForDeletionOfBookmarkWithTitle:@"Folder 1"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_notNil()];

  // Verify edit mode is closed (context bar back to default state).
  [BookmarkEarlGreyUI verifyContextBarInDefaultStateWithSelectEnabled:YES
                                                     newFolderEnabled:YES];
}

- (void)testSwipeDownToDismissFromPushedVC {
  if (!base::ios::IsRunningOnOrLater(13, 0, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 12 and lower.");
  }
  if (!IsCollectionsCardPresentationStyleEnabled()) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on when feature flag is off.");
  }

  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Open Edit Bookmark through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      performAction:grey_tap()];

  // Tap on Folder to open folder picker.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Check that Change Folder is presented.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView has been dismissed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_nil()];

  // Open EditBookmark again to verify it was cleaned up successsfully.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      performAction:grey_tap()];

  // Swipe EditBookmark down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the EditBookmark has been dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkEditViewContainerIdentifier)]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Helpers

// Verifies the Mobile Bookmarks's urls are open in the same order as they are
// in folder.
+ (void)verifyOrderOfTabsWithCurrentTabIndex:(NSUInteger)tabIndex {
  // Verify "French URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFrenchUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  // Switch to the next Tab and verify "Second URL" appears.
  // TODO(crbug.com/695749): see we if can add switchToNextTab to
  // chrome_test_util so that we don't need to pass tabIndex here.
  [ChromeEarlGrey selectTabAtIndex:tabIndex + 1];
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetSecondUrl().GetContent())]
      assertWithMatcher:grey_notNil()];

  // Switch to the next Tab and verify "First URL" appears.
  [ChromeEarlGrey selectTabAtIndex:tabIndex + 2];
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFirstUrl().GetContent())]
      assertWithMatcher:grey_notNil()];
}

@end
