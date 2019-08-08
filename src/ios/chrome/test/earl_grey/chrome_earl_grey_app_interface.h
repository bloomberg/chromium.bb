// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

#import "components/content_settings/core/common/content_settings.h"

@class ElementSelector;

// ChromeEarlGreyAppInterface contains the app-side implementation for helpers
// that primarily work via direct model access. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface ChromeEarlGreyAppInterface : NSObject

// Clears browsing history and waits for history to finish clearing before
// returning. Returns nil on success, or else an NSError indicating why the
// operation failed.
+ (NSError*)clearBrowsingHistory;

// Loads |URL| in the current WebState with transition type
// ui::PAGE_TRANSITION_TYPED and returns without waiting for the page to load.
+ (void)startLoadingURL:(NSString*)URL;

// If the current WebState is HTML content, will wait until the window ID is
// injected. Returns YES if the injection is successful or if the WebState is
// not HTML content.
+ (BOOL)waitForWindowIDInjectionIfNeeded;

// Returns YES if the current WebState is loading.
+ (BOOL)isLoading;

// Reloads the page without waiting for the page to load.
+ (void)startReloading;

#pragma mark - Tab Utilities

// Selects tab with given index in current mode (incognito or main
// (non-incognito)).
+ (void)selectTabAtIndex:(NSUInteger)index;

// Closes tab with the given index in current mode (incognito or main
// (non-incognito)).
+ (void)closeTabAtIndex:(NSUInteger)index;

// Returns YES if the browser is in incognito mode, and NO otherwise.
+ (BOOL)isIncognitoMode WARN_UNUSED_RESULT;

// Returns the number of open non-incognito tabs.
+ (NSUInteger)mainTabCount WARN_UNUSED_RESULT;

// Returns the number of open incognito tabs.
+ (NSUInteger)incognitoTabCount WARN_UNUSED_RESULT;

// Simulates a backgrounding.
// If not succeed returns an NSError indicating  why the
// operation failed, otherwise nil.
+ (NSError*)simulateTabsBackgrounding;

// Returns the number of main (non-incognito) tabs currently evicted.
+ (NSUInteger)evictedMainTabCount WARN_UNUSED_RESULT;

// Evicts the tabs associated with the non-current browser mode.
+ (void)evictOtherTabModelTabs;

// Sets the normal tabs as 'cold start' tabs
// If not succeed returns an NSError indicating  why the
// operation failed, otherwise nil.
+ (NSError*)setCurrentTabsToBeColdStartTabs;

// Resets the tab usage recorder on current mode.
// If not succeed returns an NSError indicating  why the
// operation failed, otherwise nil.
+ (NSError*)resetTabUsageRecorder;

// Opens a new tab, and does not wait for animations to complete.
+ (void)openNewTab;

// Closes current tab.
+ (void)closeCurrentTab;

// Opens a new incognito tab, and does not wait for animations to complete.
+ (void)openNewIncognitoTab;

// Closes all tabs in the current mode (incognito or normal), and does not wait
// for the UI to complete. If current mode is Incognito, mode will be switched
// normal after closing all tabs.
+ (void)closeAllTabsInCurrentMode;

// Closes all incognito tabs. If not succeed returns an NSError indicating  why
// the operation failed, otherwise nil.
+ (NSError*)closeAllIncognitoTabs;

// Closes all tabs in all modes (incognito and main (non-incognito)) and does
// not wait for the UI to complete. If current mode is Incognito, mode will be
// switched to main (non-incognito) after closing the incognito tabs.
+ (void)closeAllTabs;

// Navigates back to the previous page without waiting for the page to load.
+ (void)startGoingBack;

// Navigates forward to the next page without waiting for the page to load.
+ (void)startGoingForward;

#pragma mark - WebState Utilities (EG2)

// Attempts to tap the element with |element_id| within window.frames[0] of the
// current WebState using a JavaScript click() event. This only works on
// same-origin iframes. If not succeed returns an NSError indicating why the
// operation failed, otherwise nil.
+ (NSError*)tapWebStateElementInIFrameWithID:(NSString*)elementID;

// Taps html element with |elementID| in the current web state.
// On failure returns NO and |error| is set to include a message.
+ (BOOL)tapWebStateElementWithID:(NSString*)elementID error:(NSError*)error;

// Waits for the current web state to contain an element matching |selector|.
// If not succeed returns an NSError indicating  why the operation failed,
// otherwise nil.
+ (NSError*)waitForWebStateContainingElement:(ElementSelector*)selector;

// Waits for there to be no web state containing |text|.
// If not succeed returns an NSError indicating  why the operation failed,
// otherwise nil.
+ (NSError*)waitForWebStateNotContainingText:(NSString*)text;

// Sets value for content setting.
+ (void)setContentSettings:(ContentSetting)setting;

// Signs the user out, clears the known accounts entirely and checks whether
// the accounts were correctly removed from the keychain. Returns nil on
// success, or else an NSError indicating why the operation failed.
+ (NSError*)signOutAndClearAccounts;

// Clears the autofill profile for the given |GUID|.
+ (void)clearAutofillProfileWithGUID:(NSString*)GUID;

// Injects an autofill profile into the fake sync server with |GUID| and
// |full_name|.
+ (void)injectAutofillProfileOnFakeSyncServerWithGUID:(NSString*)GUID
                                  autofillProfileName:(NSString*)fullName;

// Returns YES if there is an autofilll profile with the corresponding |GUID|
// and |full_name|.
+ (BOOL)isAutofillProfilePresentWithGUID:(NSString*)GUID
                     autofillProfileName:(NSString*)fullName;

#pragma mark - Bookmarks Utilities (EG2)

// Waits for the bookmark internal state to be done loading.
// If not succeed returns an NSError indicating  why the operation failed,
// otherwise nil.
+ (NSError*)waitForBookmarksToFinishinLoading;

// Clears bookmarks. If not succeed returns an NSError indicating  why the
// operation failed, otherwise nil.
+ (NSError*)clearBookmarks;

#pragma mark - Sync Utilities (EG2)

// Clears fake sync server data.
+ (void)clearSyncServerData;

// Starts the sync server. The server should not be running when calling this.
+ (void)startSync;

// Stops the sync server. The server should be running when calling this.
+ (void)stopSync;

// Waits for sync to be initialized or not.
// Returns nil on success, or else an NSError indicating why the
// operation failed.
+ (NSError*)waitForSyncInitialized:(BOOL)isInitialized
                       syncTimeout:(NSTimeInterval)timeout;

// Returns the current sync cache GUID. The sync server must be running when
// calling this.
+ (NSString*)syncCacheGUID;

// Sets up a fake sync server to be used by the ProfileSyncService.
+ (void)setUpFakeSyncServer;

// Tears down the fake sync server used by the ProfileSyncService and restores
// the real one.
+ (void)tearDownFakeSyncServer;

#pragma mark - JavaScript Utilities (EG2)

// Executes JavaScript on current WebState, and waits for either the completion
// or timeout. If execution does not complete within a timeout or JavaScript
// exception is thrown, returns an NSError indicating why the operation failed,
// otherwise returns object representing execution result.
+ (id)executeJavaScript:(NSString*)javaScript error:(NSError**)error;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_
