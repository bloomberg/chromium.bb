// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_

#import <Foundation/Foundation.h>

#include <string>

#include "base/compiler_specific.h"
#import "components/content_settings/core/common/content_settings.h"
#include "components/sync/base/model_type.h"
#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"
#include "url/gurl.h"

@class ElementSelector;
@protocol GREYMatcher;

namespace chrome_test_util {

// TODO(crbug.com/788813): Evaluate if JS helpers can be consolidated.
// Execute |javascript| on current web state, and wait for either the completion
// of execution or timeout. If |out_error| is not nil, it is set to the
// error resulting from the execution, if one occurs. The return value is the
// result of the JavaScript execution. If the request is timed out, then nil is
// returned.
id ExecuteJavaScript(NSString* javascript, NSError* __autoreleasing* out_error);

}  // namespace chrome_test_util

#define ChromeEarlGrey \
  [ChromeEarlGreyImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

// Test methods that perform actions on Chrome. These methods may read or alter
// Chrome's internal state programmatically or via the UI, but in both cases
// will properly synchronize the UI for Earl Grey tests.
@interface ChromeEarlGreyImpl : BaseEGTestHelperImpl

#pragma mark - History Utilities (EG2)
// Clears browsing history. Raises an EarlGrey exception if history is not
// cleared within a timeout.
- (void)clearBrowsingHistory;

#pragma mark - Navigation Utilities (EG2)

// Loads |URL| in the current WebState with transition type
// ui::PAGE_TRANSITION_TYPED, and if waitForCompletion is YES
// waits for the loading to complete within a timeout.
// Returns nil on success, or else an NSError indicating why the operation
// failed.
- (NSError*)loadURL:(const GURL&)URL waitForCompletion:(BOOL)wait;

// Loads |URL| in the current WebState with transition type
// ui::PAGE_TRANSITION_TYPED, and waits for the loading to complete within a
// timeout.
// If the condition is not met within a timeout returns an NSError indicating
// why the operation failed, otherwise nil.
// TODO(crbug.com/963613): Change return type to avoid when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)loadURL:(const GURL&)URL;

// Returns YES if the current WebState is loading.
- (BOOL)isLoading WARN_UNUSED_RESULT;

// Reloads the page and waits for the loading to complete within a timeout, or a
// GREYAssert is induced.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)reload;

// Navigates back to the previous page and waits for the loading to complete
// within a timeout, or a GREYAssert is induced.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)goBack;

// Navigates forward to the next page and waits for the loading to complete
// within a timeout, or a GREYAssert is induced.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)goForward;

// Waits for the page to finish loading within a timeout, or a GREYAssert is
// induced.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)waitForPageToFinishLoading;

// Waits for the matcher to return an element that is sufficiently visible.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)waitForSufficientlyVisibleElementWithMatcher:
    (id<GREYMatcher>)matcher;

// Waits for there to be |count| number of non-incognito tabs within a timeout,
// or a GREYAssert is induced.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)waitForMainTabCount:(NSUInteger)count;

// Waits for there to be |count| number of incognito tabs within a timeout, or a
// GREYAssert is induced.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)waitForIncognitoTabCount:(NSUInteger)count;

#pragma mark - Settings Utilities

// Sets value for content setting.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)setContentSettings:(ContentSetting)setting;

#pragma mark - Sync Utilities

// Clears fake sync server data.
- (void)clearSyncServerData;

// Starts the sync server. The server should not be running when calling this.
- (void)startSync;

// Stops the sync server. The server should be running when calling this.
- (void)stopSync;

// Clears the autofill profile for the given |GUID|.
- (void)clearAutofillProfileWithGUID:(const std::string&)GUID;

// Injects an autofill profile into the fake sync server with |GUID| and
// |full_name|.
- (void)injectAutofillProfileOnFakeSyncServerWithGUID:(const std::string&)GUID
                                  autofillProfileName:
                                      (const std::string&)fullName;

// Returns YES if there is an autofilll profile with the corresponding |GUID|
// and |full_name|.
- (BOOL)isAutofillProfilePresentWithGUID:(const std::string&)GUID
                     autofillProfileName:(const std::string&)fullName
    WARN_UNUSED_RESULT;

// Sets up a fake sync server to be used by the ProfileSyncService.
- (void)setUpFakeSyncServer;

// Tears down the fake sync server used by the ProfileSyncService and restores
// the real one.
- (void)tearDownFakeSyncServer;

#pragma mark - Tab Utilities (EG2)

// Opens a new tab and waits for the new tab animation to complete within a
// timeout, or a GREYAssert is induced.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)openNewTab;

// Closes the current tab and waits for the UI to complete.
- (void)closeCurrentTab;

// Opens a new incognito tab and waits for the new tab animation to complete.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)openNewIncognitoTab;

// Closes all tabs in the current mode (incognito or normal), and waits for the
// UI to complete. If current mode is Incognito, mode will be switched to
// normal after closing all tabs.
- (void)closeAllTabsInCurrentMode;

// Closes all incognito tabs and waits for the UI to complete within a
// timeout, or a GREYAssert is induced.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)closeAllIncognitoTabs;

// Closes all tabs in the all modes (incognito and main (non-incognito)), and
// does not wait for the UI to complete. If current mode is Incognito, mode will
// be switched to main (non-incognito) after closing the incognito tabs.
- (void)closeAllTabs;

// Selects tab with given index in current mode (incognito or main
// (non-incognito)).
- (void)selectTabAtIndex:(NSUInteger)index;

// Closes tab with the given index in current mode (incognito or main
// (non-incognito)).
- (void)closeTabAtIndex:(NSUInteger)index;

// Returns YES if the browser is in incognito mode, and NO otherwise.
- (BOOL)isIncognitoMode WARN_UNUSED_RESULT;

// Returns the number of main (non-incognito) tabs.
- (NSUInteger)mainTabCount WARN_UNUSED_RESULT;

// Returns the number of incognito tabs.
- (NSUInteger)incognitoTabCount WARN_UNUSED_RESULT;

// Simulates a backgrounding and raises an EarlGrey exception if simulation not
// succeeded.
- (void)simulateTabsBackgrounding;

// Returns the number of main (non-incognito) tabs currently evicted.
- (NSUInteger)evictedMainTabCount WARN_UNUSED_RESULT;

// Evicts the tabs associated with the non-current browser mode.
- (void)evictOtherTabModelTabs;

// Sets the normal tabs as 'cold start' tabs and raises an EarlGrey exception if
// operation not succeeded.
- (void)setCurrentTabsToBeColdStartTabs;

// Resets the tab usage recorder on current mode and raises an EarlGrey
// exception if operation not succeeded.
- (void)resetTabUsageRecorder;

#pragma mark - SignIn Utilities

// Signs the user out, clears the known accounts entirely and checks whether the
// accounts were correctly removed from the keychain. Induces a GREYAssert if
// the operation fails.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)signOutAndClearAccounts;

#pragma mark - Sync Utilities (EG2)

// Waits for sync to be initialized or not. If not succeeded a GREYAssert is
// induced.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)waitForSyncInitialized:(BOOL)isInitialized
                       syncTimeout:(NSTimeInterval)timeout;

// Returns the current sync cache GUID. The sync server must be running when
// calling this.
- (std::string)syncCacheGUID;

#pragma mark - WebState Utilities (EG2)

// Taps html element with |elementID| in the current web state.
// A GREYAssert is induced on failure.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)tapWebStateElementWithID:(NSString*)elementID;

// Attempts to tap the element with |element_id| within window.frames[0] of the
// current WebState using a JavaScript click() event. This only works on
// same-origin iframes.
// A GREYAssert is induced on failure.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)tapWebStateElementInIFrameWithID:(const std::string&)elementID;

// Waits for the current web state to contain an element matching |selector|.
// If the condition is not met within a timeout a GREYAssert is induced.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)waitForWebStateContainingElement:(ElementSelector*)selector;

// Waits for there to be no web state containing |text|.
// If the condition is not met within a timeout a GREYAssert is induced.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)waitForWebStateNotContainingText:(std::string)text;

#pragma mark - Bookmarks Utilities (EG2)

// Waits for the bookmark internal state to be done loading.
// If the condition is not met within a timeout a GREYAssert is induced.
- (NSError*)waitForBookmarksToFinishLoading;

// Clears bookmarks if any bookmark still presents. A GREYAssert is induced if
// bookmarks can not be cleared.
- (NSError*)clearBookmarks;

#pragma mark - JavaScript Utilities (EG2)

// Executes JavaScript on current WebState, and waits for either the completion
// or timeout. If execution does not complete within a timeout a GREYAssert is
// induced.
- (id)executeJavaScript:(NSString*)javaScript;

#pragma mark - Cookie Utilities

// Returns cookies as key value pairs, where key is a cookie name and value is a
// cookie value.
// A GREYAssert is induced if cookies can not be returned.
- (NSDictionary*)cookies;

@end

// Helpers that only compile under EarlGrey 1 are included in this "EG1"
// category.
// TODO(crbug.com/922813): Update these helpers to compile under EG2 and move
// them into the main class declaration as they are converted.
@interface ChromeEarlGreyImpl (EG1)

#pragma mark - Navigation Utilities

// Waits for a static html view containing |text|. If the condition is not met
// within a timeout, a GREYAssert is induced.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)waitForStaticHTMLViewContainingText:(NSString*)text;

// Waits for there to be no static html view, or a static html view that does
// not contain |text|. If the condition is not met within a timeout, a
// GREYAssert is induced.
// TODO(crbug.com/963613): Change return type to void when
// CHROME_EG_ASSERT_NO_ERROR is removed.
- (NSError*)waitForStaticHTMLViewNotContainingText:(NSString*)text;

// Waits for a Chrome error page.
// If the condition is not met within a timeout returns an NSError indicating
// why the operation failed, otherwise nil.
- (NSError*)waitForErrorPage WARN_UNUSED_RESULT;

#pragma mark - WebState Utilities

// Attempts to submit form with |formID| in the current WebState.
- (NSError*)submitWebStateFormWithID:(const std::string&)formID
    WARN_UNUSED_RESULT;

// Waits for the current web state to contain |text|. Returns nil if the
// condition is met within a timeout, otherwise an NSError indicating why the
// operation failed.
- (NSError*)waitForWebStateContainingText:(std::string)text WARN_UNUSED_RESULT;

// Waits for there to be a web state containing a blocked |image_id|.  When
// blocked, the image element will be smaller than the actual image size.
- (NSError*)waitForWebStateContainingBlockedImageElementWithID:
    (std::string)imageID WARN_UNUSED_RESULT;

// Waits for there to be a web state containing loaded image with |image_id|.
// When loaded, the image element will have the same size as actual image.
- (NSError*)waitForWebStateContainingLoadedImageElementWithID:
    (std::string)imageID WARN_UNUSED_RESULT;

#pragma mark - Sync Utilities

// Verifies that |count| entities of the given |type| and |name| exist on the
// sync FakeServer. Folders are not included in this count. Returns nil on
// success, or else an NSError indicating why the operation failed.
- (NSError*)waitForSyncServerEntitiesWithType:(syncer::ModelType)type
                                         name:(const std::string&)name
                                        count:(size_t)count
                                      timeout:(NSTimeInterval)timeout
    WARN_UNUSED_RESULT;

// Gets the number of entities of the given |type|.
- (int)numberOfSyncEntitiesWithType:(syncer::ModelType)type WARN_UNUSED_RESULT;

// Injects a bookmark into the fake sync server with |URL| and |title|.
- (void)injectBookmarkOnFakeSyncServerWithURL:(const std::string&)URL
                                bookmarkTitle:(const std::string&)title;

// Adds typed URL into HistoryService.
- (void)addTypedURL:(const GURL&)URL;

// Triggers a sync cycle for a |type|.
- (void)triggerSyncCycleForType:(syncer::ModelType)type;

// If the provided |url| is present (or not) if |expected_present|
// is YES (or NO) returns nil, otherwise an NSError indicating why the operation
// failed.
- (NSError*)waitForTypedURL:(const GURL&)URL
              expectPresent:(BOOL)expectPresent
                    timeout:(NSTimeInterval)timeout WARN_UNUSED_RESULT;

// Deletes typed URL from HistoryService.
- (void)deleteTypedURL:(const GURL&)URL;

// Injects typed URL to sync FakeServer.
- (void)injectTypedURLOnFakeSyncServer:(const std::string&)URL;

// Deletes an autofill profile from the fake sync server with |GUID|, if it
// exists. If it doesn't exist, nothing is done.
- (void)deleteAutofillProfileOnFakeSyncServerWithGUID:(const std::string&)GUID;

// Verifies the sessions hierarchy on the Sync FakeServer. |expected_urls| is
// the collection of URLs that are to be expected for a single window. Returns
// nil on success, or else an NSError indicating why the operation failed. See
// the SessionsHierarchy class for documentation regarding the verification.
- (NSError*)verifySyncServerURLs:(const std::multiset<std::string>&)URLs
    WARN_UNUSED_RESULT;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_
