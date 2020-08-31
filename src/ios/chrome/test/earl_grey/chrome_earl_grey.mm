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
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/nserror_util.h"
#include "ios/web/public/test/element_selector.h"
#include "net/base/mac/url_conversions.h"

#if defined(CHROME_EARL_GREY_1)
#import <WebKit/WebKit.h>

#import "ios/chrome/test/app/browsing_data_test_util.h"          // nogncheck
#import "ios/chrome/test/app/chrome_test_util.h"                 // nogncheck
#include "ios/chrome/test/app/navigation_test_util.h"            // nogncheck
#import "ios/chrome/test/app/sync_test_util.h"                   // nogncheck
#import "ios/chrome/test/app/tab_test_util.h"                    // nogncheck
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"  // nogncheck
#import "ios/web/public/test/earl_grey/js_test_util.h"           // nogncheck
#import "ios/web/public/test/web_view_content_test_util.h"       // nogncheck
#import "ios/web/public/test/web_view_interaction_test_util.h"   // nogncheck
#import "ios/web/public/web_state.h"                             // nogncheck
#endif

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

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

#if defined(CHROME_EARL_GREY_2)
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(ChromeEarlGreyAppInterface)
#endif  // defined(CHROME_EARL_GREY_2)

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

#pragma mark - Device Utilities

- (void)rotateDeviceToOrientation:(UIDeviceOrientation)deviceOrientation
                            error:(NSError**)error {
#if defined(CHROME_EARL_GREY_1)
  NSError* strongErrorReference = nil;
  [EarlGrey rotateDeviceToOrientation:deviceOrientation
                           errorOrNil:&strongErrorReference];
  if (error)
    *error = strongErrorReference;
#elif defined(CHROME_EARL_GREY_2)
  [EarlGrey rotateDeviceToOrientation:deviceOrientation error:error];
#else
#error Neither CHROME_EARL_GREY_1 nor CHROME_EARL_GREY_2 are defined
#endif
}

- (BOOL)isKeyboardShownWithError:(NSError**)error {
  return
#if defined(CHROME_EARL_GREY_1)
      [GREYKeyboard isKeyboardShown];
#elif defined(CHROME_EARL_GREY_2)
      [EarlGrey isKeyboardShownWithError:error];
#else
#error Neither CHROME_EARL_GREY_1 nor CHROME_EARL_GREY_2 are defined
#endif
}

- (BOOL)isIPadIdiom {
#if defined(CHROME_EARL_GREY_1)
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
#elif defined(CHROME_EARL_GREY_2)
  UIUserInterfaceIdiom idiom =
      [[GREY_REMOTE_CLASS_IN_APP(UIDevice) currentDevice] userInterfaceIdiom];
#endif

  return idiom == UIUserInterfaceIdiomPad;
}

- (BOOL)isCompactWidth {
  UIUserInterfaceSizeClass horizontalSpace =
#if defined(CHROME_EARL_GREY_1)
      [[[[UIApplication sharedApplication] keyWindow] traitCollection]
          horizontalSizeClass];
#elif defined(CHROME_EARL_GREY_2)
      [[[[GREY_REMOTE_CLASS_IN_APP(UIApplication) sharedApplication] keyWindow]
          traitCollection] horizontalSizeClass];
#endif
  return horizontalSpace == UIUserInterfaceSizeClassCompact;
}

- (BOOL)isCompactHeight {
  UIUserInterfaceSizeClass verticalSpace =
#if defined(CHROME_EARL_GREY_1)
      [[[[UIApplication sharedApplication] keyWindow] traitCollection]
          verticalSizeClass];
#elif defined(CHROME_EARL_GREY_2)
      [[[[GREY_REMOTE_CLASS_IN_APP(UIApplication) sharedApplication] keyWindow]
          traitCollection] verticalSizeClass];
#endif
  return verticalSpace == UIUserInterfaceSizeClassCompact;
}

- (BOOL)isSplitToolbarMode {
  return [self isCompactWidth] && ![self isCompactHeight];
}

- (BOOL)isRegularXRegularSizeClass {
  UITraitCollection* traitCollection =
#if defined(CHROME_EARL_GREY_1)
      [[[UIApplication sharedApplication] keyWindow] traitCollection];
#elif defined(CHROME_EARL_GREY_2)
      [[[GREY_REMOTE_CLASS_IN_APP(UIApplication) sharedApplication] keyWindow]
          traitCollection];
#endif
  return traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular &&
         traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular;
}

#pragma mark - History Utilities (EG2)

- (void)clearBrowsingHistory {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface clearBrowsingHistory]);

  // After clearing browsing history via code, wait for the UI to be done
  // with any updates. This includes icons from the new tab page being removed.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (NSInteger)getBrowsingHistoryEntryCount {
  return [ChromeEarlGreyAppInterface getBrowsingHistoryEntryCount];
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
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (BOOL)isIncognitoMode {
  return [ChromeEarlGreyAppInterface isIncognitoMode];
}

- (void)closeTabAtIndex:(NSUInteger)index {
  [ChromeEarlGreyAppInterface closeTabAtIndex:index];
  // Tab changes are initiated through |WebStateList|. Need to wait its
  // obeservers to complete UI changes at app.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (NSUInteger)mainTabCount {
  return [ChromeEarlGreyAppInterface mainTabCount];
}

- (NSUInteger)incognitoTabCount {
  return [ChromeEarlGreyAppInterface incognitoTabCount];
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
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
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
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (void)simulateAddAccountFromWeb {
  [ChromeEarlGreyAppInterface simulateAddAccountFromWeb];
  [self waitForPageToFinishLoading];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (void)closeCurrentTab {
  [ChromeEarlGreyAppInterface closeCurrentTab];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (void)openNewIncognitoTab {
  [ChromeEarlGreyAppInterface openNewIncognitoTab];
  [self waitForPageToFinishLoading];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (void)closeAllTabsInCurrentMode {
  [ChromeEarlGreyAppInterface closeAllTabsInCurrentMode];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (void)closeAllNormalTabs {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface closeAllNormalTabs]);
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (void)closeAllIncognitoTabs {
  EG_TEST_HELPER_ASSERT_NO_ERROR(
      [ChromeEarlGreyAppInterface closeAllIncognitoTabs]);
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (void)closeAllTabs {
  [ChromeEarlGreyAppInterface closeAllTabs];
  // Tab changes are initiated through |WebStateList|. Need to wait its
  // obeservers to complete UI changes at app.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (void)waitForPageToFinishLoading {
  GREYCondition* finishedLoading = [GREYCondition
      conditionWithName:kWaitForPageToFinishLoadingError
                  block:^{
                    return ![ChromeEarlGreyAppInterface isLoading];
                  }];

  bool pageLoaded = [finishedLoading waitWithTimeout:kWaitForPageLoadTimeout];
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
  [ChromeEarlGrey waitForAndTapButton:chrome_test_util::TabGridOpenButton()];
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
    NSArray* cookieNameValue = [nameValuePair componentsSeparatedByString:@"="];
    EG_TEST_HELPER_ASSERT_TRUE((2 == cookieNameValue.count),
                               @"Cookie has invalid format.");
    NSString* cookieName = cookieNameValue[0];
    NSString* cookieValue = cookieNameValue[1];
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
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

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
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

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
  [ChromeEarlGrey showTabSwitcher];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  [ChromeEarlGrey
      waitForAndTapButton:chrome_test_util::TabGridCloseAllButton()];
  [ChromeEarlGrey
      waitForAndTapButton:chrome_test_util::TabGridUndoCloseAllButton()];
  [ChromeEarlGrey waitForAndTapButton:chrome_test_util::TabGridDoneButton()];
  [self waitForRestoreSessionToFinish];
  [self waitForPageToFinishLoading];
}

- (BOOL)webStateWebViewUsesContentInset {
  return [ChromeEarlGreyAppInterface webStateWebViewUsesContentInset];
}

- (CGSize)webStateWebViewSize {
  return [ChromeEarlGreyAppInterface webStateWebViewSize];
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

- (void)signOutAndClearAccounts {
  [self signOutAndClearIdentities];
}

- (void)addBookmarkWithSyncPassphrase:(NSString*)syncPassphrase {
  [ChromeEarlGreyAppInterface addBookmarkWithSyncPassphrase:syncPassphrase];
}

#pragma mark - Bookmarks Utilities (EG2)

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

- (BOOL)isCreditCardScannerEnabled {
  return [ChromeEarlGreyAppInterface isCreditCardScannerEnabled];
}

- (BOOL)isAutofillCompanyNameEnabled {
  return [ChromeEarlGreyAppInterface isAutofillCompanyNameEnabled];
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

@end

// The helpers below only compile under EarlGrey1.
// TODO(crbug.com/922813): Update these helpers to compile under EG2 and move
// them into the main class declaration as they are converted.
#if defined(CHROME_EARL_GREY_1)

namespace chrome_test_util {

id ExecuteJavaScript(NSString* javascript,
                     NSError* __autoreleasing* out_error) {
  __block bool did_complete = false;
  __block id result = nil;
  __block NSError* temp_error = nil;
  CRWJSInjectionReceiver* evaluator =
      chrome_test_util::GetCurrentWebState()->GetJSInjectionReceiver();
  [evaluator executeJavaScript:javascript
             completionHandler:^(id value, NSError* error) {
               did_complete = true;
               result = [value copy];
               temp_error = [error copy];
             }];

  bool success =
      WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
        return did_complete;
      });
  if (!success)
    return nil;
  if (out_error) {
    NSError* __autoreleasing auto_released_error = temp_error;
    *out_error = auto_released_error;
  }
  return result;
}

}  // namespace chrome_test_util

@implementation ChromeEarlGreyImpl (EG1)

@end

#endif  // defined(CHROME_EARL_GREY_1)
