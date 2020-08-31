// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::BookmarkHomeDoneButton;
using chrome_test_util::BookmarksNavigationBarBackButton;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;

// Bookmark promo integration tests for Chrome.
@interface BookmarksPromoTestCase : ChromeTestCase
@end

@implementation BookmarksPromoTestCase

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

#pragma mark - BookmarksPromoTestCase Tests

// Tests that the promo view is only seen at root level and not in any of the
// child nodes.
- (void)testPromoViewIsSeenOnlyInRootNode {
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];

  // We are going to set the PromoAlreadySeen preference. Set a teardown handler
  // to reset it.
  [self setTearDownHandler:^{
    [BookmarkEarlGrey setPromoAlreadySeen:NO];
  }];
  // Check that sign-in promo view is visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];

  // Go to child node.
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Wait until promo is gone.
  [SigninEarlGreyUI checkSigninPromoNotVisible];

  // Check that the promo already seen state is not updated.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];

  // Come back to root node, and the promo view should appear.
  [[EarlGrey selectElementWithMatcher:BookmarksNavigationBarBackButton()]
      performAction:grey_tap()];

  // Check promo view is still visible.
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tapping No thanks on the promo make it disappear.
- (void)testPromoNoThanksMakeItDisappear {
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];

  // We are going to set the PromoAlreadySeen preference. Set a teardown handler
  // to reset it.
  [self setTearDownHandler:^{
    [BookmarkEarlGrey setPromoAlreadySeen:NO];
  }];
  // Check that sign-in promo view is visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];

  // Tap the dismiss button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSigninPromoCloseButtonId),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Wait until promo is gone.
  [SigninEarlGreyUI checkSigninPromoNotVisible];

  // Check that the promo already seen state is updated.
  [BookmarkEarlGrey verifyPromoAlreadySeen:YES];
}

// Tests the tapping on the primary button of sign-in promo view in a cold
// state makes the sign-in sheet appear, and the promo still appears after
// dismissing the sheet.
- (void)testSignInPromoWithColdStateUsingPrimaryButton {
  [BookmarkEarlGreyUI openBookmarks];

  // Check that sign-in promo view are visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];

  // Tap the primary button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Cancel the sign-in operation.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSkipSigninAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Check that the bookmarks UI reappeared and the cell is still here.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];
}

// Tests the tapping on the primary button of sign-in promo view in a warm
// state makes the confirmaiton sheet appear, and the promo still appears after
// dismissing the sheet.
- (void)testSignInPromoWithWarmStateUsingPrimaryButton {
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];

  // Set up a fake identity.
  [BookmarkEarlGrey setupFakeIdentity];

  // Check that promo is visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState];

  // Tap the primary button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Cancel the sign-in operation.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSkipSigninAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Check that the bookmarks UI reappeared and the cell is still here.
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState];

  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
}

// Tests the tapping on the secondary button of sign-in promo view in a warm
// state makes the sign-in sheet appear, and the promo still appears after
// dismissing the sheet.
- (void)testSignInPromoWithWarmStateUsingSecondaryButton {
  [BookmarkEarlGrey setupStandardBookmarks];
  [BookmarkEarlGreyUI openBookmarks];
  // Set up a fake identity.
  NSString* identityEmail = [BookmarkEarlGrey setupFakeIdentity];

  // Check that sign-in promo view are visible.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState];

  // Tap the secondary button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Select the identity to dismiss the identity chooser.
  [SigninEarlGreyUI selectIdentityWithEmail:identityEmail];

  // Tap the CANCEL button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSkipSigninAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Check that the bookmarks UI reappeared and the cell is still here.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState];
}

// Tests that the sign-in promo should not be shown after been shown 19 times.
- (void)testAutomaticSigninPromoDismiss {
  [BookmarkEarlGrey setPromoAlreadySeenNumberOfTimes:19];
  [BookmarkEarlGreyUI openBookmarks];
  // Check the sign-in promo view is visible.
  [SigninEarlGreyUI
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];
  // Check the sign-in promo already-seen state didn't change.
  [BookmarkEarlGrey verifyPromoAlreadySeen:NO];
  GREYAssertEqual(20, [BookmarkEarlGrey numberOfTimesPromoAlreadySeen],
                  @"Should have incremented the display count");

  // Close the bookmark view and open it again.
  [[EarlGrey selectElementWithMatcher:BookmarkHomeDoneButton()]
      performAction:grey_tap()];
  [BookmarkEarlGreyUI openBookmarks];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  // Check that the sign-in promo is not visible anymore.
  [SigninEarlGreyUI checkSigninPromoNotVisible];
}

@end
