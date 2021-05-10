// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"

#import <Foundation/Foundation.h>

#include "base/format_macros.h"
#include "base/json/json_string_value_serializer.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/nserror_util.h"
#include "ios/web/public/test/element_selector.h"
#include "net/base/mac/url_conversions.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::ActivityViewHeader;
using chrome_test_util::CopyLinkButton;
using chrome_test_util::OpenLinkInNewTabButton;
using chrome_test_util::OpenLinkInIncognitoButton;
using chrome_test_util::OpenLinkInNewWindowButton;
using chrome_test_util::ShareButton;

namespace {
NSString* const kWaitForPageToStartLoadingError = @"Page did not start to load";
NSString* const kWaitForPageToFinishLoadingError =
    @"Page did not finish loading";
NSString* const kTypedURLError =
    @"Error occurred during typed URL verification.";
NSString* const kWaitForRestoreSessionToFinishError =
    @"Session restoration did not finish";
}

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(ChromeEarlGreyAppInterface)

@interface ChromeEarlGreyImpl ()

// Waits for session restoration to finish within a timeout, or a GREYAssert is
// induced.
- (void)waitForRestoreSessionToFinish;

// Perform a tap with a timeout, or a GREYAssert is induced. Occasionally EG
// doesn't sync up properly to the animations of tab switcher, so it is
// necessary to poll.
- (void)waitForAndTapButton:(id<GREYMatcher>)button;

@end

@implementation ChromeEarlGreyImpl

#pragma mark - Test Utilities

- (void)waitForMatcher:(id<GREYMatcher>)matcher {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_notNil()
                                                             error:&error];
    return error == nil;
  };
  NSString* errorString =
      [NSString stringWithFormat:@"Waiting for matcher %@ failed.", matcher];
  EG_TEST_HELPER_ASSERT_TRUE(
      base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForUIElementTimeout, condition),
      errorString);
}

#pragma mark - Device Utilities

- (BOOL)isIPadIdiom {
  UIUserInterfaceIdiom idiom =
      [[GREY_REMOTE_CLASS_IN_APP(UIDevice) currentDevice] userInterfaceIdiom];

  return idiom == UIUserInterfaceIdiomPad;
}

- (BOOL)isCompactWidth {
  UIUserInterfaceSizeClass horizontalSpace =
      [[[[GREY_REMOTE_CLASS_IN_APP(UIApplication) sharedApplication] keyWindow]
          traitCollection] horizontalSizeClass];
  return horizontalSpace == UIUserInterfaceSizeClassCompact;
}

- (BOOL)isCompactHeight {
  UIUserInterfaceSizeClass verticalSpace =
      [[[[GREY_REMOTE_CLASS_IN_APP(UIApplication) sharedApplication] keyWindow]
          traitCollection] verticalSizeClass];
  return verticalSpace == UIUserInterfaceSizeClassCompact;
}

- (BOOL)isSplitToolbarMode {
  return [self isCompactWidth] && ![self isCompactHeight];
}

- (BOOL)isRegularXRegularSizeClass {
  UITraitCollection* traitCollection =
      [[[GREY_REMOTE_CLASS_IN_APP(UIApplication) sharedApplication] keyWindow]
          traitCollection];
  return traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular &&
         traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular;
}

#pragma mark - History Utilities (EG2)

- (void)clearBrowsingHistory {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface clearBrowsingHistory]);

  // After clearing browsing history via code, wait for the UI to be done
  // with any updates. This includes icons from the new tab page being removed.
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (NSInteger)browsingHistoryEntryCount {
  NSError* error = nil;
  NSInteger result =
      [ChromeEarlGreyAppInterface browsingHistoryEntryCountWithError:&error];
  EG_TEST_HELPER_ASSERT_NO_ERROR(error);
  return result;
}

- (NSInteger)navigationBackListItemsCount {
  return [ChromeEarlGreyAppInterface navigationBackListItemsCount];
}

- (void)removeBrowsingCache {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface removeBrowsingCache]);
}

#pragma mark - Navigation Utilities (EG2)

- (void)goBack {
  [ChromeEarlGreyAppInterface startGoingBack];

  [self waitForPageToFinishLoading];
}

- (void)goForward {
  [ChromeEarlGreyAppInterface startGoingForward];
  [self waitForPageToFinishLoading];
}

- (void)reload {
  [self reloadAndWaitForCompletion:YES];
}

- (void)reloadAndWaitForCompletion:(BOOL)wait {
  [ChromeEarlGreyAppInterface startReloading];
  if (wait) {
    [self waitForPageToFinishLoading];
  }
}

- (void)openURLFromExternalApp:(const GURL&)URL {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  [ChromeEarlGreyAppInterface openURLFromExternalApp:spec];
}

- (void)dismissSettings {
  [ChromeEarlGreyAppInterface dismissSettings];
}

#pragma mark - Tab Utilities (EG2)

- (void)selectTabAtIndex:(NSUInteger)index {
  [ChromeEarlGreyAppInterface selectTabAtIndex:index];
  // Tab changes are initiated through |WebStateList|. Need to wait its
  // obeservers to complete UI changes at app.
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (BOOL)isIncognitoMode {
  return [ChromeEarlGreyAppInterface isIncognitoMode];
}

- (void)closeTabAtIndex:(NSUInteger)index {
  [ChromeEarlGreyAppInterface closeTabAtIndex:index];
  // Tab changes are initiated through |WebStateList|. Need to wait its
  // obeservers to complete UI changes at app.
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (NSUInteger)mainTabCount {
  return [ChromeEarlGreyAppInterface mainTabCount];
}

- (NSUInteger)incognitoTabCount {
  return [ChromeEarlGreyAppInterface incognitoTabCount];
}

- (NSUInteger)browserCount {
  return [ChromeEarlGreyAppInterface browserCount];
}

- (NSUInteger)evictedMainTabCount {
  return [ChromeEarlGreyAppInterface evictedMainTabCount];
}

- (void)evictOtherTabModelTabs {
  [ChromeEarlGreyAppInterface evictOtherTabModelTabs];
}

- (void)simulateTabsBackgrounding {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface simulateTabsBackgrounding]);
}

- (void)saveSessionImmediately {
  [ChromeEarlGreyAppInterface saveSessionImmediately];

  // Saving is always performed on a separate thread, so spin the run loop a
  // bit to ensure save.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSeconds(1));
}

- (void)setCurrentTabsToBeColdStartTabs {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface setCurrentTabsToBeColdStartTabs]);
}

- (void)resetTabUsageRecorder {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface resetTabUsageRecorder]);
}

- (void)openNewTab {
  [ChromeEarlGreyAppInterface openNewTab];
  [self waitForPageToFinishLoading];
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)simulateExternalAppURLOpening {
  NSURL* openedNSURL =
      [ChromeEarlGreyAppInterface simulateExternalAppURLOpening];
  // Wait until the navigation is finished.
  GURL openedGURL = net::GURLWithNSURL(openedNSURL);
  GREYCondition* finishedLoading = [GREYCondition
      conditionWithName:kWaitForPageToStartLoadingError
                  block:^{
                    return openedGURL == [ChromeEarlGrey webStateVisibleURL];
                  }];
  bool pageLoaded = [finishedLoading waitWithTimeout:kWaitForPageLoadTimeout];
  EG_TEST_HELPER_ASSERT_TRUE(pageLoaded, kWaitForPageToStartLoadingError);
  // Wait until the page is loaded.
  [self waitForPageToFinishLoading];
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)simulateAddAccountFromWeb {
  [ChromeEarlGreyAppInterface simulateAddAccountFromWeb];
  [self waitForPageToFinishLoading];
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)closeCurrentTab {
  [ChromeEarlGreyAppInterface closeCurrentTab];
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)openNewIncognitoTab {
  [ChromeEarlGreyAppInterface openNewIncognitoTab];
  [self waitForPageToFinishLoading];
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)closeAllTabsInCurrentMode {
  [ChromeEarlGreyAppInterface closeAllTabsInCurrentMode];
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)closeAllNormalTabs {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface closeAllNormalTabs]);
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)closeAllIncognitoTabs {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface closeAllIncognitoTabs]);
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)closeAllTabs {
  [ChromeEarlGreyAppInterface closeAllTabs];
  // Tab changes are initiated through |WebStateList|. Need to wait its
  // obeservers to complete UI changes at app.
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)waitForPageToFinishLoading {
  GREYCondition* finishedLoading = [GREYCondition
      conditionWithName:kWaitForPageToFinishLoadingError
                  block:^{
                    return ![ChromeEarlGreyAppInterface isLoading];
                  }];

  BOOL pageLoaded = [finishedLoading waitWithTimeout:kWaitForPageLoadTimeout];
  EG_TEST_HELPER_ASSERT_TRUE(pageLoaded, kWaitForPageToFinishLoadingError);
}

- (void)applicationOpenURL:(const GURL&)URL {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  [ChromeEarlGreyAppInterface applicationOpenURL:spec];
}

- (void)loadURL:(const GURL&)URL waitForCompletion:(BOOL)wait {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  [ChromeEarlGreyAppInterface startLoadingURL:spec];
  if (wait) {
    [self waitForPageToFinishLoading];
    EG_TEST_HELPER_ASSERT_TRUE(
        [ChromeEarlGreyAppInterface waitForWindowIDInjectionIfNeeded],
        @"WindowID failed to inject");
  }
}

- (void)loadURL:(const GURL&)URL {
  return [self loadURL:URL waitForCompletion:YES];
}

- (BOOL)isLoading {
  return [ChromeEarlGreyAppInterface isLoading];
}

- (void)waitForSufficientlyVisibleElementWithMatcher:(id<GREYMatcher>)matcher {
  NSString* errorDescription = [NSString
      stringWithFormat:
          @"Failed waiting for element with matcher %@ to become visible",
          matcher];

  GREYCondition* waitForElement = [GREYCondition
      conditionWithName:errorDescription
                  block:^{
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:matcher]
                        assertWithMatcher:grey_sufficientlyVisible()
                                    error:&error];
                    return error == nil;
                  }];

  bool matchedElement =
      [waitForElement waitWithTimeout:kWaitForUIElementTimeout];
  EG_TEST_HELPER_ASSERT_TRUE(matchedElement, errorDescription);
}

- (void)waitForUIElementToAppearWithMatcher:(id<GREYMatcher>)matcher {
  NSString* errorDescription = [NSString
      stringWithFormat:@"Failed waiting for element with matcher %@ to appear",
                       matcher];

  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_notNil()
                                                             error:&error];
    return error == nil;
  };

  bool matched =
      WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition);
  GREYAssert(matched, errorDescription);
}

- (void)waitForUIElementToDisappearWithMatcher:(id<GREYMatcher>)matcher {
  NSString* errorDescription = [NSString
      stringWithFormat:
          @"Failed waiting for element with matcher %@ to disappear", matcher];

  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_nil()
                                                             error:&error];
    return error == nil;
  };

  bool matched =
      WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition);
  GREYAssert(matched, errorDescription);
}

- (NSString*)currentTabTitle {
  return [ChromeEarlGreyAppInterface currentTabTitle];
}

- (NSString*)nextTabTitle {
  return [ChromeEarlGreyAppInterface nextTabTitle];
}

- (NSString*)currentTabID {
  return [ChromeEarlGreyAppInterface currentTabID];
}

- (NSString*)nextTabID {
  return [ChromeEarlGreyAppInterface nextTabID];
}

- (void)waitForAndTapButton:(id<GREYMatcher>)button {
  NSString* errorDescription =
      [NSString stringWithFormat:@"Waiting to tap on button %@", button];
  // Perform a tap with a timeout. Occasionally EG doesn't sync up properly to
  // the animations of tab switcher, so it is necessary to poll here.
  // TODO(crbug.com/1050052): Fix the underlying issue in EarlGrey and remove
  // this workaround.
  GREYCondition* tapButton =
      [GREYCondition conditionWithName:errorDescription
                                 block:^BOOL {
                                   NSError* error;
                                   [[EarlGrey selectElementWithMatcher:button]
                                       performAction:grey_tap()
                                               error:&error];
                                   return error == nil;
                                 }];
  // Wait for the tap.
  BOOL hasClicked = [tapButton waitWithTimeout:kWaitForUIElementTimeout];
  EG_TEST_HELPER_ASSERT_TRUE(hasClicked, errorDescription);
}

- (void)showTabSwitcher {
  [ChromeEarlGrey waitForAndTapButton:chrome_test_util::ShowTabsButton()];
}

#pragma mark - Cookie Utilities (EG2)

- (NSDictionary*)cookies {
  NSString* const kGetCookiesScript =
      @"document.cookie ? document.cookie.split(/;\\s*/) : [];";
  id result = [self executeJavaScript:kGetCookiesScript];
  // TODO(crbug.com/1041000): Assert that |result| is iterable using
  // respondToSelector instead of methodSignatureForSelector, after upgrading to
  // the EG version which handles selectors.
  EG_TEST_HELPER_ASSERT_TRUE(
      [result methodSignatureForSelector:@selector(objectEnumerator)],
      @"The script response is not iterable.");

  NSMutableDictionary* cookies = [NSMutableDictionary dictionary];
  for (NSString* nameValuePair in result) {
    NSMutableArray* cookieNameValue =
        [[nameValuePair componentsSeparatedByString:@"="] mutableCopy];
    // For cookies with multiple parameters it may be valid to have multiple
    // occurrences of the delimiter.
    EG_TEST_HELPER_ASSERT_TRUE((2 <= cookieNameValue.count),
                               @"Cookie has invalid format.");
    NSString* cookieName = cookieNameValue[0];
    [cookieNameValue removeObjectAtIndex:0];

    NSString* cookieValue = [cookieNameValue componentsJoinedByString:@"="];
    cookies[cookieName] = cookieValue;
  }

  return cookies;
}

#pragma mark - WebState Utilities (EG2)

- (void)tapWebStateElementWithID:(NSString*)elementID {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface tapWebStateElementWithID:elementID]);
}

- (void)tapWebStateElementInIFrameWithID:(const std::string&)elementID {
  NSString* NSElementID = base::SysUTF8ToNSString(elementID);
  EG_TEST_HELPER_ASSERT_NO_ERROR([ChromeEarlGreyAppInterface
      tapWebStateElementInIFrameWithID:NSElementID]);
}

- (void)waitForWebStateContainingElement:(ElementSelector*)selector {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface waitForWebStateContainingElement:selector]);
}

- (void)waitForMainTabCount:(NSUInteger)count {
  __block NSUInteger actualCount = [ChromeEarlGreyAppInterface mainTabCount];
  NSString* conditionName = [NSString
      stringWithFormat:@"Waiting for main tab count to become %" PRIuNS, count];

  // Allow the UI to become idle, in case any tabs are being opened or closed.
  GREYWaitForAppToIdle(@"App failed to idle");

  GREYCondition* tabCountCheck = [GREYCondition
      conditionWithName:conditionName
                  block:^{
                    actualCount = [ChromeEarlGreyAppInterface mainTabCount];
                    return actualCount == count;
                  }];
  bool tabCountEqual = [tabCountCheck waitWithTimeout:kWaitForUIElementTimeout];

  NSString* errorString = [NSString
      stringWithFormat:@"Failed waiting for main tab count to become %" PRIuNS
                        "; actual count: %" PRIuNS,
                       count, actualCount];
  EG_TEST_HELPER_ASSERT_TRUE(tabCountEqual, errorString);
}

- (void)waitForIncognitoTabCount:(NSUInteger)count {
  NSString* errorString = [NSString
      stringWithFormat:
          @"Failed waiting for incognito tab count to become %" PRIuNS, count];

  // Allow the UI to become idle, in case any tabs are being opened or closed.
  GREYWaitForAppToIdle(@"App failed to idle");

  GREYCondition* tabCountCheck = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    return
                        [ChromeEarlGreyAppInterface incognitoTabCount] == count;
                  }];
  bool tabCountEqual = [tabCountCheck waitWithTimeout:kWaitForUIElementTimeout];
  EG_TEST_HELPER_ASSERT_TRUE(tabCountEqual, errorString);
}

- (NSUInteger)indexOfActiveNormalTab {
  return [ChromeEarlGreyAppInterface indexOfActiveNormalTab];
}

- (void)waitForRestoreSessionToFinish {
  GREYCondition* finishedRestoreSession = [GREYCondition
      conditionWithName:kWaitForRestoreSessionToFinishError
                  block:^{
                    return !
                        [ChromeEarlGreyAppInterface isRestoreSessionInProgress];
                  }];
  bool restoreSessionCompleted =
      [finishedRestoreSession waitWithTimeout:kWaitForPageLoadTimeout];
  EG_TEST_HELPER_ASSERT_TRUE(restoreSessionCompleted,
                             kWaitForRestoreSessionToFinishError);
}

- (void)submitWebStateFormWithID:(const std::string&)UTF8FormID {
  NSString* formID = base::SysUTF8ToNSString(UTF8FormID);
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface submitWebStateFormWithID:formID]);
}

- (void)waitForWebStateContainingText:(const std::string&)UTF8Text {
  [self waitForWebStateContainingText:UTF8Text timeout:kWaitForPageLoadTimeout];
}

- (void)waitForWebStateFrameContainingText:(const std::string&)UTF8Text {
  NSString* text = base::SysUTF8ToNSString(UTF8Text);
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface waitForWebStateContainingTextInIFrame:text]);
}

- (void)waitForWebStateContainingText:(const std::string&)UTF8Text
                              timeout:(NSTimeInterval)timeout {
  NSString* text = base::SysUTF8ToNSString(UTF8Text);
  NSString* errorString = [NSString
      stringWithFormat:@"Failed waiting for web state containing %@", text];

  GREYCondition* waitForText = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    return
                        [ChromeEarlGreyAppInterface webStateContainsText:text];
                  }];
  bool containsText = [waitForText waitWithTimeout:timeout];
  EG_TEST_HELPER_ASSERT_TRUE(containsText, errorString);
}

- (void)waitForWebStateNotContainingText:(const std::string&)UTF8Text {
  NSString* text = base::SysUTF8ToNSString(UTF8Text);
  NSString* errorString = [NSString
      stringWithFormat:@"Failed waiting for web state not containing %@", text];

  GREYCondition* waitForText = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    return !
                        [ChromeEarlGreyAppInterface webStateContainsText:text];
                  }];
  bool containsText = [waitForText waitWithTimeout:kWaitForUIElementTimeout];
  EG_TEST_HELPER_ASSERT_TRUE(containsText, errorString);
}

- (void)waitForWebStateContainingBlockedImageElementWithID:
    (const std::string&)UTF8ImageID {
  NSString* imageID = base::SysUTF8ToNSString(UTF8ImageID);
  EG_TEST_HELPER_ASSERT_NO_ERROR([ChromeEarlGreyAppInterface
      waitForWebStateContainingBlockedImage:imageID]);
}

- (void)waitForWebStateContainingLoadedImageElementWithID:
    (const std::string&)UTF8ImageID {
  NSString* imageID = base::SysUTF8ToNSString(UTF8ImageID);
  EG_TEST_HELPER_ASSERT_NO_ERROR([ChromeEarlGreyAppInterface
      waitForWebStateContainingLoadedImage:imageID]);
}

- (GURL)webStateVisibleURL {
  return GURL(
      base::SysNSStringToUTF8([ChromeEarlGreyAppInterface webStateVisibleURL]));
}

- (GURL)webStateLastCommittedURL {
  return GURL(base::SysNSStringToUTF8(
      [ChromeEarlGreyAppInterface webStateLastCommittedURL]));
}

- (void)purgeCachedWebViewPages {
  [ChromeEarlGreyAppInterface purgeCachedWebViewPages];
  [self waitForRestoreSessionToFinish];
  [self waitForPageToFinishLoading];
}

- (void)triggerRestoreViaTabGridRemoveAllUndo {
  [ChromeEarlGreyAppInterface disableCloseAllTabsConfirmation];

  [ChromeEarlGrey showTabSwitcher];
  GREYWaitForAppToIdle(@"App failed to idle");
  [ChromeEarlGrey
      waitForAndTapButton:chrome_test_util::TabGridCloseAllButton()];
  [ChromeEarlGrey
      waitForAndTapButton:chrome_test_util::TabGridUndoCloseAllButton()];
  [ChromeEarlGrey waitForAndTapButton:chrome_test_util::TabGridDoneButton()];
  [self waitForRestoreSessionToFinish];
  [self waitForPageToFinishLoading];

  [ChromeEarlGreyAppInterface resetCloseAllTabsConfirmation];
}

- (BOOL)webStateWebViewUsesContentInset {
  return [ChromeEarlGreyAppInterface webStateWebViewUsesContentInset];
}

- (CGSize)webStateWebViewSize {
  return [ChromeEarlGreyAppInterface webStateWebViewSize];
}

- (void)stopAllWebStatesLoading {
  [ChromeEarlGreyAppInterface stopAllWebStatesLoading];
  // Wait for any UI change.
  GREYWaitForAppToIdle(
      @"Failed to wait app to idle after stopping all WebStates");
}

- (void)clearAllWebStateBrowsingData {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface clearAllWebStateBrowsingData]);
}

#pragma mark - Settings Utilities (EG2)

- (void)setContentSettings:(ContentSetting)setting {
  [ChromeEarlGreyAppInterface setContentSettings:setting];
}

#pragma mark - Sync Utilities (EG2)

- (void)clearSyncServerData {
  [ChromeEarlGreyAppInterface clearSyncServerData];
}

- (void)startSync {
  [ChromeEarlGreyAppInterface startSync];
}

- (void)stopSync {
  [ChromeEarlGreyAppInterface stopSync];
}

- (void)
    addUserDemographicsToSyncServerWithBirthYear:(int)rawBirthYear
                                          gender:
                                              (metrics::UserDemographicsProto::
                                                   Gender)gender {
  [ChromeEarlGreyAppInterface
      addUserDemographicsToSyncServerWithBirthYear:rawBirthYear
                                            gender:gender];
}

- (void)clearAutofillProfileWithGUID:(const std::string&)UTF8GUID {
  NSString* GUID = base::SysUTF8ToNSString(UTF8GUID);
  [ChromeEarlGreyAppInterface clearAutofillProfileWithGUID:GUID];
}

- (void)addAutofillProfileToFakeSyncServerWithGUID:(const std::string&)UTF8GUID
                               autofillProfileName:
                                   (const std::string&)UTF8FullName {
  NSString* GUID = base::SysUTF8ToNSString(UTF8GUID);
  NSString* fullName = base::SysUTF8ToNSString(UTF8FullName);
  [ChromeEarlGreyAppInterface
      addAutofillProfileToFakeSyncServerWithGUID:GUID
                             autofillProfileName:fullName];
}

- (BOOL)isAutofillProfilePresentWithGUID:(const std::string&)UTF8GUID
                     autofillProfileName:(const std::string&)UTF8FullName {
  NSString* GUID = base::SysUTF8ToNSString(UTF8GUID);
  NSString* fullName = base::SysUTF8ToNSString(UTF8FullName);
  return [ChromeEarlGreyAppInterface isAutofillProfilePresentWithGUID:GUID
                                                  autofillProfileName:fullName];
}

- (void)setUpFakeSyncServer {
  [ChromeEarlGreyAppInterface setUpFakeSyncServer];
}

- (void)tearDownFakeSyncServer {
  [ChromeEarlGreyAppInterface tearDownFakeSyncServer];
}

- (int)numberOfSyncEntitiesWithType:(syncer::ModelType)type {
  return [ChromeEarlGreyAppInterface numberOfSyncEntitiesWithType:type];
}

- (void)addFakeSyncServerBookmarkWithURL:(const GURL&)URL
                                   title:(const std::string&)UTF8Title {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  NSString* title = base::SysUTF8ToNSString(UTF8Title);
  [ChromeEarlGreyAppInterface addFakeSyncServerBookmarkWithURL:spec
                                                         title:title];
}

- (void)addFakeSyncServerLegacyBookmarkWithURL:(const GURL&)URL
                                         title:(const std::string&)UTF8Title
                     originator_client_item_id:
                         (const std::string&)UTF8OriginatorClientItemId {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  NSString* title = base::SysUTF8ToNSString(UTF8Title);
  NSString* originator_client_item_id =
      base::SysUTF8ToNSString(UTF8OriginatorClientItemId);
  [ChromeEarlGreyAppInterface
      addFakeSyncServerLegacyBookmarkWithURL:spec
                                       title:title
                   originator_client_item_id:originator_client_item_id];
}

- (void)addFakeSyncServerTypedURL:(const GURL&)URL {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  [ChromeEarlGreyAppInterface addFakeSyncServerTypedURL:spec];
}

- (void)addHistoryServiceTypedURL:(const GURL&)URL {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  [ChromeEarlGreyAppInterface addHistoryServiceTypedURL:spec];
}

- (void)deleteHistoryServiceTypedURL:(const GURL&)URL {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  [ChromeEarlGreyAppInterface deleteHistoryServiceTypedURL:spec];
}

- (void)waitForTypedURL:(const GURL&)URL
          expectPresent:(BOOL)expectPresent
                timeout:(NSTimeInterval)timeout {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  GREYCondition* waitForTypedURL =
      [GREYCondition conditionWithName:kTypedURLError
                                 block:^{
                                   return [ChromeEarlGreyAppInterface
                                            isTypedURL:spec
                                       presentOnClient:expectPresent];
                                 }];

  bool success = [waitForTypedURL waitWithTimeout:timeout];
  EG_TEST_HELPER_ASSERT_TRUE(success, kTypedURLError);
}

- (void)waitForSyncInvalidationFields {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface waitForSyncInvalidationFields]);
}

- (void)triggerSyncCycleForType:(syncer::ModelType)type {
  [ChromeEarlGreyAppInterface triggerSyncCycleForType:type];
}

- (void)deleteAutofillProfileFromFakeSyncServerWithGUID:
    (const std::string&)UTF8GUID {
  NSString* GUID = base::SysUTF8ToNSString(UTF8GUID);
  [ChromeEarlGreyAppInterface
      deleteAutofillProfileFromFakeSyncServerWithGUID:GUID];
}

- (void)waitForSyncInitialized:(BOOL)isInitialized
                   syncTimeout:(NSTimeInterval)timeout {
  EG_TEST_HELPER_ASSERT_NO_ERROR([ChromeEarlGreyAppInterface
      waitForSyncInitialized:isInitialized
                 syncTimeout:timeout]);
}

- (const std::string)syncCacheGUID {
  NSString* cacheGUID = [ChromeEarlGreyAppInterface syncCacheGUID];
  return base::SysNSStringToUTF8(cacheGUID);
}

- (void)verifySyncServerURLs:(NSArray<NSString*>*)URLs {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface verifySessionsOnSyncServerWithSpecs:URLs]);
}

- (void)waitForSyncServerEntitiesWithType:(syncer::ModelType)type
                                     name:(const std::string&)UTF8Name
                                    count:(size_t)count
                                  timeout:(NSTimeInterval)timeout {
  NSString* errorString = [NSString
      stringWithFormat:@"Expected %zu entities of the %d type.", count, type];
  NSString* name = base::SysUTF8ToNSString(UTF8Name);
  GREYCondition* verifyEntities = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    NSError* error = [ChromeEarlGreyAppInterface
                        verifyNumberOfSyncEntitiesWithType:type
                                                      name:name
                                                     count:count];
                    return !error;
                  }];

  bool success = [verifyEntities waitWithTimeout:timeout];
  EG_TEST_HELPER_ASSERT_TRUE(success, errorString);
}

#pragma mark - Window utilities (EG2)

- (CGRect)screenPositionOfScreenWithNumber:(int)windowNumber {
  return [ChromeEarlGreyAppInterface
      screenPositionOfScreenWithNumber:windowNumber];
}

- (NSUInteger)windowCount WARN_UNUSED_RESULT {
  return [ChromeEarlGreyAppInterface windowCount];
}

- (NSUInteger)foregroundWindowCount WARN_UNUSED_RESULT {
  return [ChromeEarlGreyAppInterface foregroundWindowCount];
}

- (void)closeAllExtraWindows {
  if ([self windowCount] <= 1) {
    return;
  }
  [ChromeEarlGreyAppInterface closeAllExtraWindows];

  // Tab changes are initiated through |WebStateList|. Need to wait its
  // observers to complete UI changes at app. Wait until window count is
  // officially 1 in the app, otherwise we may start a new test while the
  // removed window is still partly registered.
  [self waitForForegroundWindowCount:1];
}

- (void)waitForForegroundWindowCount:(NSUInteger)count {
  __block NSUInteger actualCount =
      [ChromeEarlGreyAppInterface foregroundWindowCount];
  NSString* conditionName = [NSString
      stringWithFormat:@"Waiting for window count to become %" PRIuNS, count];

  // Allow the UI to become idle, in case any tabs are being opened or closed.
  GREYWaitForAppToIdle(@"App failed to idle");

  GREYCondition* browserCountCheck = [GREYCondition
      conditionWithName:conditionName
                  block:^{
                    actualCount =
                        [ChromeEarlGreyAppInterface foregroundWindowCount];
                    return actualCount == count;
                  }];
  bool browserCountEqual =
      [browserCountCheck waitWithTimeout:kWaitForUIElementTimeout];

  NSString* errorString = [NSString
      stringWithFormat:@"Failed waiting for window count to become %" PRIuNS
                        "; actual count: %" PRIuNS,
                       count, actualCount];
  EG_TEST_HELPER_ASSERT_TRUE(browserCountEqual, errorString);
}

- (void)openNewWindow {
  EG_TEST_HELPER_ASSERT_NO_ERROR([ChromeEarlGreyAppInterface openNewWindow]);
}

- (void)openNewTabInWindowWithNumber:(int)windowNumber {
  [ChromeEarlGreyAppInterface openNewTabInWindowWithNumber:windowNumber];
  [self waitForPageToFinishLoadingInWindowWithNumber:windowNumber];
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)closeWindowWithNumber:(int)windowNumber {
  [ChromeEarlGreyAppInterface closeWindowWithNumber:windowNumber];
}

- (void)changeWindowWithNumber:(int)windowNumber
                   toNewNumber:(int)newWindowNumber {
  [ChromeEarlGreyAppInterface changeWindowWithNumber:windowNumber
                                         toNewNumber:newWindowNumber];
}

- (void)waitForPageToFinishLoadingInWindowWithNumber:(int)windowNumber {
  GREYCondition* finishedLoading = [GREYCondition
      conditionWithName:kWaitForPageToFinishLoadingError
                  block:^{
                    return ![ChromeEarlGreyAppInterface
                        isLoadingInWindowWithNumber:windowNumber];
                  }];

  BOOL pageLoaded = [finishedLoading waitWithTimeout:kWaitForPageLoadTimeout];
  EG_TEST_HELPER_ASSERT_TRUE(pageLoaded, kWaitForPageToFinishLoadingError);
}

- (void)loadURL:(const GURL&)URL
    inWindowWithNumber:(int)windowNumber
     waitForCompletion:(BOOL)wait {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  [ChromeEarlGreyAppInterface startLoadingURL:spec
                           inWindowWithNumber:windowNumber];
  if (wait) {
    [self waitForPageToFinishLoadingInWindowWithNumber:windowNumber];
    EG_TEST_HELPER_ASSERT_TRUE(
        [ChromeEarlGreyAppInterface
            waitForWindowIDInjectionIfNeededInWindowWithNumber:windowNumber],
        @"WindowID failed to inject");
  }
}

- (void)loadURL:(const GURL&)URL inWindowWithNumber:(int)windowNumber {
  return [self loadURL:URL
      inWindowWithNumber:windowNumber
       waitForCompletion:YES];
}

- (BOOL)isLoadingInWindowWithNumber:(int)windowNumber {
  return [ChromeEarlGreyAppInterface isLoadingInWindowWithNumber:windowNumber];
}

- (void)waitForWebStateContainingText:(const std::string&)UTF8Text
                   inWindowWithNumber:(int)windowNumber {
  [self waitForWebStateContainingText:UTF8Text
                              timeout:kWaitForPageLoadTimeout
                   inWindowWithNumber:windowNumber];
}

- (void)waitForWebStateContainingText:(const std::string&)UTF8Text
                              timeout:(NSTimeInterval)timeout
                   inWindowWithNumber:(int)windowNumber {
  NSString* text = base::SysUTF8ToNSString(UTF8Text);
  NSString* errorString =
      [NSString stringWithFormat:@"Failed waiting for web state containing %@ "
                                 @"in window with number %d",
                                 text, windowNumber];

  GREYCondition* waitForText =
      [GREYCondition conditionWithName:errorString
                                 block:^{
                                   return [ChromeEarlGreyAppInterface
                                       webStateContainsText:text
                                         inWindowWithNumber:windowNumber];
                                 }];
  bool containsText = [waitForText waitWithTimeout:timeout];
  EG_TEST_HELPER_ASSERT_TRUE(containsText, errorString);
}

- (void)waitForMainTabCount:(NSUInteger)count
         inWindowWithNumber:(int)windowNumber {
  __block NSUInteger actualCount =
      [ChromeEarlGreyAppInterface mainTabCountInWindowWithNumber:windowNumber];
  NSString* conditionName = [NSString
      stringWithFormat:@"Waiting for main tab count to become %" PRIuNS
                        " from %" PRIuNS " in window with number %d",
                       count, actualCount, windowNumber];

  // Allow the UI to become idle, in case any tabs are being opened or closed.
  GREYWaitForAppToIdle(@"App failed to idle");

  GREYCondition* tabCountCheck = [GREYCondition
      conditionWithName:conditionName
                  block:^{
                    actualCount = [ChromeEarlGreyAppInterface
                        mainTabCountInWindowWithNumber:windowNumber];
                    return actualCount == count;
                  }];
  bool tabCountEqual = [tabCountCheck waitWithTimeout:kWaitForUIElementTimeout];

  NSString* errorString = [NSString
      stringWithFormat:@"Failed waiting for main tab count to become %" PRIuNS
                        " in window with number %d"
                        "; actual count: %" PRIuNS,
                       count, windowNumber, actualCount];
  EG_TEST_HELPER_ASSERT_TRUE(tabCountEqual, errorString);
}

- (void)waitForIncognitoTabCount:(NSUInteger)count
              inWindowWithNumber:(int)windowNumber {
  __block NSUInteger actualCount = [ChromeEarlGreyAppInterface
      incognitoTabCountInWindowWithNumber:windowNumber];
  NSString* conditionName =
      [NSString stringWithFormat:
                    @"Failed waiting for incognito tab count to become %" PRIuNS
                     " from %" PRIuNS " in window with number %d",
                    count, actualCount, windowNumber];

  // Allow the UI to become idle, in case any tabs are being opened or closed.
  GREYWaitForAppToIdle(@"App failed to idle");

  GREYCondition* tabCountCheck = [GREYCondition
      conditionWithName:conditionName
                  block:^{
                    actualCount = [ChromeEarlGreyAppInterface
                        incognitoTabCountInWindowWithNumber:windowNumber];
                    return actualCount == count;
                  }];
  bool tabCountEqual = [tabCountCheck waitWithTimeout:kWaitForUIElementTimeout];

  NSString* errorString =
      [NSString stringWithFormat:
                    @"Failed waiting for incognito tab count to become %" PRIuNS
                     " in window with number %d"
                     "; actual count: %" PRIuNS,
                    count, windowNumber, actualCount];
  EG_TEST_HELPER_ASSERT_TRUE(tabCountEqual, errorString);
}

#pragma mark - SignIn Utilities (EG2)

- (void)signOutAndClearIdentities {
  [ChromeEarlGreyAppInterface signOutAndClearIdentities];

  GREYCondition* allIdentitiesCleared = [GREYCondition
      conditionWithName:@"All Chrome identities were cleared"
                  block:^{
                    return ![ChromeEarlGreyAppInterface hasIdentities];
                  }];
  bool success = [allIdentitiesCleared waitWithTimeout:kWaitForActionTimeout];
  EG_TEST_HELPER_ASSERT_TRUE(success,
                             @"Failed waiting for identities to be cleared");
}

#pragma mark - Bookmarks Utilities (EG2)

- (void)addBookmarkWithSyncPassphrase:(NSString*)syncPassphrase {
  [ChromeEarlGreyAppInterface addBookmarkWithSyncPassphrase:syncPassphrase];
}

- (void)waitForBookmarksToFinishLoading {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface waitForBookmarksToFinishinLoading]);
}

- (void)clearBookmarks {
  EG_TEST_HELPER_ASSERT_NO_ERROR([ChromeEarlGreyAppInterface clearBookmarks]);
}

- (id)executeJavaScript:(NSString*)JS {
  NSError* error = nil;
  id result = [ChromeEarlGreyAppInterface executeJavaScript:JS error:&error];
  EG_TEST_HELPER_ASSERT_NO_ERROR(error);
  return result;
}

- (NSString*)mobileUserAgentString {
  return [ChromeEarlGreyAppInterface mobileUserAgentString];
}

#pragma mark - URL Utilities (EG2)

- (NSString*)displayTitleForURL:(const GURL&)URL {
  NSString* spec = base::SysUTF8ToNSString(URL.spec());
  return [ChromeEarlGreyAppInterface displayTitleForURL:spec];
}

#pragma mark - Accessibility Utilities (EG2)

- (void)verifyAccessibilityForCurrentScreen {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface verifyAccessibilityForCurrentScreen]);
}

#pragma mark - Check features (EG2)

- (BOOL)isBlockNewTabPagePendingLoadEnabled {
  return [ChromeEarlGreyAppInterface isBlockNewTabPagePendingLoadEnabled];
}

- (BOOL)isVariationEnabled:(int)variationID {
  return [ChromeEarlGreyAppInterface isVariationEnabled:variationID];
}

- (BOOL)isTriggerVariationEnabled:(int)variationID {
  return [ChromeEarlGreyAppInterface isTriggerVariationEnabled:variationID];
}

- (BOOL)isUMACellularEnabled {
  return [ChromeEarlGreyAppInterface isUMACellularEnabled];
}

- (BOOL)isUKMEnabled {
  return [ChromeEarlGreyAppInterface isUKMEnabled];
}

- (BOOL)isTestFeatureEnabled {
  return [ChromeEarlGreyAppInterface isTestFeatureEnabled];
}

- (BOOL)isDemographicMetricsReportingEnabled {
  return [ChromeEarlGreyAppInterface isDemographicMetricsReportingEnabled];
}

- (BOOL)appHasLaunchSwitch:(const std::string&)launchSwitch {
  return [ChromeEarlGreyAppInterface
      appHasLaunchSwitch:base::SysUTF8ToNSString(launchSwitch)];
}

- (BOOL)isCustomWebKitLoadedIfRequested {
  return [ChromeEarlGreyAppInterface isCustomWebKitLoadedIfRequested];
}

- (BOOL)isMobileModeByDefault {
  return [ChromeEarlGreyAppInterface isMobileModeByDefault];
}

- (BOOL)isIllustratedEmptyStatesEnabled {
  return [ChromeEarlGreyAppInterface isIllustratedEmptyStatesEnabled];
}

- (BOOL)isNativeContextMenusEnabled {
  return [ChromeEarlGreyAppInterface isNativeContextMenusEnabled];
}

- (BOOL)areMultipleWindowsSupported {
  return [ChromeEarlGreyAppInterface areMultipleWindowsSupported];
}

- (BOOL)isCloseAllTabsConfirmationEnabled {
  return [ChromeEarlGreyAppInterface isCloseAllTabsConfirmationEnabled];
}

#pragma mark - ScopedBlockPopupsPref

- (ContentSetting)popupPrefValue {
  return [ChromeEarlGreyAppInterface popupPrefValue];
}

- (void)setPopupPrefValue:(ContentSetting)value {
  return [ChromeEarlGreyAppInterface setPopupPrefValue:value];
}

- (NSInteger)registeredKeyCommandCount {
  return [ChromeEarlGreyAppInterface registeredKeyCommandCount];
}

- (void)simulatePhysicalKeyboardEvent:(NSString*)input
                                flags:(UIKeyModifierFlags)flags {
  [ChromeEarlGreyAppInterface simulatePhysicalKeyboardEvent:input flags:flags];
}

#pragma mark - Pref Utilities (EG2)

// Returns a base::Value representation of the requested pref.
- (std::unique_ptr<base::Value>)localStatePrefValue:
    (const std::string&)prefName {
  std::string jsonRepresentation =
      base::SysNSStringToUTF8([ChromeEarlGreyAppInterface
          localStatePrefValue:base::SysUTF8ToNSString(prefName)]);
  JSONStringValueDeserializer deserializer(jsonRepresentation);
  return deserializer.Deserialize(/*error_code=*/nullptr,
                                  /*error_message=*/nullptr);
}

- (bool)localStateBooleanPref:(const std::string&)prefName {
  std::unique_ptr<base::Value> value = [self localStatePrefValue:prefName];
  BOOL success = value && value->is_bool();
  EG_TEST_HELPER_ASSERT_TRUE(success, @"Expected bool");
  return success ? value->GetBool() : false;
}

- (int)localStateIntegerPref:(const std::string&)prefName {
  std::unique_ptr<base::Value> value = [self localStatePrefValue:prefName];
  BOOL success = value && value->is_int();
  EG_TEST_HELPER_ASSERT_TRUE(success, @"Expected int");
  return success ? value->GetInt() : 0;
}

- (std::string)localStateStringPref:(const std::string&)prefName {
  std::unique_ptr<base::Value> value = [self localStatePrefValue:prefName];
  BOOL success = value && value->is_string();
  EG_TEST_HELPER_ASSERT_TRUE(success, @"Expected string");
  return success ? value->GetString() : "";
}

// Returns a base::Value representation of the requested pref.
- (std::unique_ptr<base::Value>)userPrefValue:(const std::string&)prefName {
  std::string jsonRepresentation =
      base::SysNSStringToUTF8([ChromeEarlGreyAppInterface
          userPrefValue:base::SysUTF8ToNSString(prefName)]);
  JSONStringValueDeserializer deserializer(jsonRepresentation);
  return deserializer.Deserialize(/*error_code=*/nullptr,
                                  /*error_message=*/nullptr);
}

- (bool)userBooleanPref:(const std::string&)prefName {
  std::unique_ptr<base::Value> value = [self userPrefValue:prefName];
  BOOL success = value && value->is_bool();
  EG_TEST_HELPER_ASSERT_TRUE(success, @"Expected bool");
  return success ? value->GetBool() : false;
}

- (int)userIntegerPref:(const std::string&)prefName {
  std::unique_ptr<base::Value> value = [self userPrefValue:prefName];
  BOOL success = value && value->is_int();
  EG_TEST_HELPER_ASSERT_TRUE(success, @"Expected int");
  return success ? value->GetInt() : 0;
}

- (std::string)userStringPref:(const std::string&)prefName {
  std::unique_ptr<base::Value> value = [self userPrefValue:prefName];
  BOOL success = value && value->is_string();
  EG_TEST_HELPER_ASSERT_TRUE(success, @"Expected string");
  return success ? value->GetString() : "";
}

- (void)setBoolValue:(BOOL)value forUserPref:(const std::string&)UTF8PrefName {
  NSString* prefName = base::SysUTF8ToNSString(UTF8PrefName);
  return [ChromeEarlGreyAppInterface setBoolValue:value forUserPref:prefName];
}

- (void)resetBrowsingDataPrefs {
  return [ChromeEarlGreyAppInterface resetBrowsingDataPrefs];
}

#pragma mark - Pasteboard Utilities (EG2)

- (void)verifyStringCopied:(NSString*)text {
  ConditionBlock condition = ^{
    return !![[ChromeEarlGreyAppInterface pasteboardString]
        containsString:text];
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(kWaitForActionTimeout,
                                                          condition),
             @"Waiting for '%@' to be copied to pasteboard.", text);
}

- (GURL)pasteboardURL {
  NSString* absoluteString = [ChromeEarlGreyAppInterface pasteboardURLSpec];
  return absoluteString ? GURL(base::SysNSStringToUTF8(absoluteString))
                        : GURL::EmptyGURL();
}

#pragma mark - Context Menus Utilities (EG2)

- (void)verifyCopyLinkActionWithText:(NSString*)text
                        useNewString:(BOOL)useNewString {
  [ChromeEarlGreyAppInterface clearPasteboardURLs];
  [[EarlGrey selectElementWithMatcher:CopyLinkButton(useNewString)]
      performAction:grey_tap()];
  [self verifyStringCopied:text];
}

- (void)verifyOpenInNewTabActionWithURL:(const std::string&)URL {
  // Check tab count prior to execution.
  NSUInteger oldRegularTabCount = [ChromeEarlGreyAppInterface mainTabCount];
  NSUInteger oldIncognitoTabCount =
      [ChromeEarlGreyAppInterface incognitoTabCount];

  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];

  [self waitForMainTabCount:oldRegularTabCount + 1];
  [self waitForIncognitoTabCount:oldIncognitoTabCount];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(URL)]
      assertWithMatcher:grey_notNil()];
}

- (void)verifyOpenInNewWindowActionWithContent:(const std::string&)content {
  [ChromeEarlGrey waitForForegroundWindowCount:1];
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewWindowButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGrey waitForWebStateContainingText:content inWindowWithNumber:1];
}

- (void)verifyOpenInIncognitoActionWithURL:(const std::string&)URL
                              useNewString:(BOOL)useNewString {
  // Check tab count prior to execution.
  NSUInteger oldRegularTabCount = [ChromeEarlGreyAppInterface mainTabCount];
  NSUInteger oldIncognitoTabCount =
      [ChromeEarlGreyAppInterface incognitoTabCount];

  [[EarlGrey selectElementWithMatcher:OpenLinkInIncognitoButton(useNewString)]
      performAction:grey_tap()];

  [self waitForIncognitoTabCount:oldIncognitoTabCount + 1];
  [self waitForMainTabCount:oldRegularTabCount];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(URL)]
      assertWithMatcher:grey_notNil()];
}

- (void)verifyShareActionWithPageTitle:(NSString*)pageTitle {
  [[EarlGrey selectElementWithMatcher:ShareButton()] performAction:grey_tap()];

  // Page title is added asynchronously, so wait for its appearance.
  [self waitForMatcher:grey_allOf(ActivityViewHeader(pageTitle),
                                  grey_sufficientlyVisible(), nil)];

  // Dismiss the Activity View by tapping outside its bounds.
  [[EarlGrey selectElementWithMatcher:grey_keyWindow()]
      performAction:grey_tap()];
}

#pragma mark - Unified consent utilities

- (void)setURLKeyedAnonymizedDataCollectionEnabled:(BOOL)enabled {
  return [ChromeEarlGreyAppInterface
      setURLKeyedAnonymizedDataCollectionEnabled:enabled];
}

#pragma mark - Watcher utilities

- (void)watchForButtonsWithLabels:(NSArray<NSString*>*)labels
                          timeout:(NSTimeInterval)timeout {
  [ChromeEarlGreyAppInterface watchForButtonsWithLabels:labels timeout:timeout];
}

- (BOOL)watcherDetectedButtonWithLabel:(NSString*)label {
  return [ChromeEarlGreyAppInterface watcherDetectedButtonWithLabel:label];
}

- (void)stopWatcher {
  [ChromeEarlGreyAppInterface stopWatcher];
}

@end
