// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "components/content_settings/core/common/content_settings.h"
#import "components/sync/base/model_type.h"
#include "third_party/metrics_proto/user_demographics.pb.h"

@class ElementSelector;
@class NamedGuide;

// ChromeEarlGreyAppInterface contains the app-side implementation for helpers
// that primarily work via direct model access. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface ChromeEarlGreyAppInterface : NSObject

// Clears browsing history and waits for history to finish clearing before
// returning. Returns nil on success, or else an NSError indicating why the
// operation failed.
+ (NSError*)clearBrowsingHistory;

// Returns the number of entries in the history database. Returns -1 if there
// was an error.
+ (NSInteger)getBrowsingHistoryEntryCount;

// Clears browsing cache. Returns nil on success, or else an NSError indicating
// the operation failed.
+ (NSError*)removeBrowsingCache;

// Opens |URL| using the application delegate.
+ (void)applicationOpenURL:(NSString*)spec;

// Loads the URL |spec| in the current WebState with transition type
// ui::PAGE_TRANSITION_TYPED and returns without waiting for the page to load.
+ (void)startLoadingURL:(NSString*)spec;

// If the current WebState is HTML content, will wait until the window ID is
// injected. Returns YES if the injection is successful or if the WebState is
// not HTML content.
+ (BOOL)waitForWindowIDInjectionIfNeeded;

// Returns YES if the current WebState is loading.
+ (BOOL)isLoading;

// Reloads the page without waiting for the page to load.
+ (void)startReloading;

// Returns the NamedGuide with the given |name|, if one is attached to |view|
// or one of |view|'s ancestors.  If no guide is found, returns nil.
+ (NamedGuide*)guideWithName:(NSString*)name view:(UIView*)view;

// Loads |URL| as if it was opened from an external application.
+ (void)openURLFromExternalApp:(NSString*)URL;

// Programmatically dismisses settings screen.
+ (void)dismissSettings;

#pragma mark - Tab Utilities (EG2)

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

// Persists the current list of tabs to disk immediately.
+ (void)saveSessionImmediately;

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

// Simulates opening http://www.example.com/ from another application.
// Returns the opened URL.
+ (NSURL*)simulateExternalAppURLOpening;

// Simulates opening the add account sign-in flow from the web.
+ (void)simulateAddAccountFromWeb;

// Closes current tab.
+ (void)closeCurrentTab;

// Opens a new incognito tab, and does not wait for animations to complete.
+ (void)openNewIncognitoTab;

// Closes all tabs in the current mode (incognito or normal), and does not wait
// for the UI to complete. If current mode is Incognito, mode will be switched
// normal after closing all tabs.
+ (void)closeAllTabsInCurrentMode;

// Closes all normal (non-incognito) tabs. If not succeed returns an NSError
// indicating why the operation failed, otherwise nil.
+ (NSError*)closeAllNormalTabs;

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

// Returns the title of the current selected tab.
+ (NSString*)currentTabTitle;

// Returns the title of the next tab. Assumes that there is a next tab.
+ (NSString*)nextTabTitle;

// Returns a unique identifier for the current Tab.
+ (NSString*)currentTabID;

// Returns a unique identifier for the next Tab.
+ (NSString*)nextTabID;

// Returns the index of active tab in normal mode.
+ (NSUInteger)indexOfActiveNormalTab;

#pragma mark - WebState Utilities (EG2)

// Attempts to tap the element with |element_id| within window.frames[0] of the
// current WebState using a JavaScript click() event. This only works on
// same-origin iframes. If not succeed returns an NSError indicating why the
// operation failed, otherwise nil.
+ (NSError*)tapWebStateElementInIFrameWithID:(NSString*)elementID;

// Taps html element with |elementID| in the current web state.
// If not succeed returns an NSError indicating why the
// operation failed, otherwise nil.
+ (NSError*)tapWebStateElementWithID:(NSString*)elementID;

// Waits for the current web state to contain an element matching |selector|.
// If not succeed returns an NSError indicating  why the operation failed,
// otherwise nil.
+ (NSError*)waitForWebStateContainingElement:(ElementSelector*)selector;

// Waits for the current web state's frames to contain |text|.
// If not succeed returns an NSError indicating  why the operation failed,
// otherwise nil.
+ (NSError*)waitForWebStateContainingTextInIFrame:(NSString*)text;

// Attempts to submit form with |formID| in the current WebState.
// Returns nil on success, or else an NSError indicating why the operation
// failed.
+ (NSError*)submitWebStateFormWithID:(NSString*)formID;

// Returns YES if the current WebState contains |text|.
+ (BOOL)webStateContainsText:(NSString*)text;

// Waits for the current WebState to contain loaded image with |imageID|.
// When loaded, the image element will have the same size as actual image.
// Returns nil if the condition is met within a timeout, or else an NSError
// indicating why the operation failed.
+ (NSError*)waitForWebStateContainingLoadedImage:(NSString*)imageID;

// Waits for the current WebState to contain a blocked image with |imageID|.
// When blocked, the image element will be smaller than the actual image size.
// Returns nil if the condition is met within a timeout, or else an NSError
// indicating why the operation failed.
+ (NSError*)waitForWebStateContainingBlockedImage:(NSString*)imageID;

// Sets value for content setting.
+ (void)setContentSettings:(ContentSetting)setting;

// Signs the user out from Chrome and then starts clearing the identities.
//
// Note: This method does not wait for identities to be cleared from the
// keychain. To wait for this operation to finish, please use an GREYCondition
// and wait for +hasIdentities to return NO.
+ (void)signOutAndClearIdentities;

// Returns YES if there is at at least identity in the ChromeIdentityService.
+ (BOOL)hasIdentities;

// Returns the current WebState's VisibleURL.
+ (NSString*)webStateVisibleURL;

// Returns the current WebState's last committed URL.
+ (NSString*)webStateLastCommittedURL;

// Purges cached web view pages in the current web state, so the next time back
// navigation will not use a cached page. Browsers don't have to use a fresh
// version for back/forward navigation for HTTP pages and may serve a version
// from the cache even if the Cache-Control response header says otherwise.
+ (void)purgeCachedWebViewPages;

// Returns YES if the current WebState's navigation manager is currently
// restoring session state.
+ (BOOL)isRestoreSessionInProgress;

// Returns YES if the current WebState's web view uses the content inset to
// correctly align the top of the content with the bottom of the top bar.
+ (BOOL)webStateWebViewUsesContentInset;

// Returns the size of the current WebState's web view.
+ (CGSize)webStateWebViewSize;

#pragma mark - Bookmarks Utilities (EG2)

// Waits for the bookmark internal state to be done loading.
// If not succeed returns an NSError indicating  why the operation failed,
// otherwise nil.
+ (NSError*)waitForBookmarksToFinishinLoading;

// Clears bookmarks. If not succeed returns an NSError indicating  why the
// operation failed, otherwise nil.
+ (NSError*)clearBookmarks;

#pragma mark - URL Utilities (EG2)

// Returns the title string to be used for a page with |URL| if that page
// doesn't specify a title.
+ (NSString*)displayTitleForURL:(NSString*)URL;

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

// Gets the number of entities of the given |type|.
+ (int)numberOfSyncEntitiesWithType:(syncer::ModelType)type;

// Injects a bookmark into the fake sync server with |URL| and |title|.
+ (void)addFakeSyncServerBookmarkWithURL:(NSString*)URL title:(NSString*)title;

// Injects a legacy bookmark into the fake sync server. The legacy bookmark
// means 2015 and earlier, prior to the adoption of GUIDs for originator client
// item ID.
+ (void)addFakeSyncServerLegacyBookmarkWithURL:(NSString*)URL
                                         title:(NSString*)title
                     originator_client_item_id:
                         (NSString*)originator_client_item_id;

// Injects typed URL to sync FakeServer.
+ (void)addFakeSyncServerTypedURL:(NSString*)URL;

// Adds typed URL into HistoryService.
+ (void)addHistoryServiceTypedURL:(NSString*)URL;

// Deletes typed URL from HistoryService.
+ (void)deleteHistoryServiceTypedURL:(NSString*)URL;

// If the provided URL |spec| is either present or not present in HistoryService
// (depending on |expectPresent|), return YES. If the present status of |spec|
// is not what is expected, or there is an error, return NO.
+ (BOOL)isTypedURL:(NSString*)spec presentOnClient:(BOOL)expectPresent;

// Triggers a sync cycle for a |type|.
+ (void)triggerSyncCycleForType:(syncer::ModelType)type;

// Injects user demographics into the fake sync server. |rawBirthYear| is the
// true birth year, pre-noise, and the gender corresponds to the proto enum
// UserDemographicsProto::Gender.
+ (void)
    addUserDemographicsToSyncServerWithBirthYear:(int)rawBirthYear
                                          gender:
                                              (metrics::UserDemographicsProto::
                                                   Gender)gender;

// Clears the autofill profile for the given |GUID|.
+ (void)clearAutofillProfileWithGUID:(NSString*)GUID;

// Injects an autofill profile into the fake sync server with |GUID| and
// |full_name|.
+ (void)addAutofillProfileToFakeSyncServerWithGUID:(NSString*)GUID
                               autofillProfileName:(NSString*)fullName;

// Returns YES if there is an autofilll profile with the corresponding |GUID|
// and |full_name|.
+ (BOOL)isAutofillProfilePresentWithGUID:(NSString*)GUID
                     autofillProfileName:(NSString*)fullName;

// Deletes an autofill profile from the fake sync server with |GUID|, if it
// exists. If it doesn't exist, nothing is done.
+ (void)deleteAutofillProfileFromFakeSyncServerWithGUID:(NSString*)GUID;

// Verifies the sessions hierarchy on the Sync FakeServer. |specs| is
// the collection of URLs that are to be expected for a single window. On
// failure, returns a NSError describing the failure. See the
// SessionsHierarchy class for documentation regarding the verification.
+ (NSError*)verifySessionsOnSyncServerWithSpecs:(NSArray<NSString*>*)specs;

// Verifies that |count| entities of the given |type| and |name| exist on the
// sync FakeServer. Folders are not included in this count. Returns NSError
// if there is a failure or if the count does not match.
+ (NSError*)verifyNumberOfSyncEntitiesWithType:(NSUInteger)type
                                          name:(NSString*)name
                                         count:(NSUInteger)count;

// Adds a bookmark with a sync passphrase. The sync server will need the sync
// passphrase to start.
+ (void)addBookmarkWithSyncPassphrase:(NSString*)syncPassphrase;

#pragma mark - JavaScript Utilities (EG2)

// Executes JavaScript on current WebState, and waits for either the completion
// or timeout. If execution does not complete within a timeout or JavaScript
// exception is thrown, returns an NSError indicating why the operation failed,
// otherwise returns object representing execution result.
+ (id)executeJavaScript:(NSString*)javaScript error:(NSError**)error;

// Returns the user agent that should be used for the mobile version.
+ (NSString*)mobileUserAgentString;

#pragma mark - Accessibility Utilities (EG2)

// Verifies that all interactive elements on screen (or at least one of their
// descendants) are accessible.
+ (NSError*)verifyAccessibilityForCurrentScreen WARN_UNUSED_RESULT;

#pragma mark - Check features (EG2)

// Helpers for checking feature state. These can't use FeatureList directly when
// invoked from test code, as the EG test code runs in a separate process and
// must query Chrome for the state.

// Returns YES if BlockNewTabPagePendingLoad feature is enabled.
+ (BOOL)isBlockNewTabPagePendingLoadEnabled WARN_UNUSED_RESULT;

// Returns YES if |variationID| is enabled.
+ (BOOL)isVariationEnabled:(int)variationID;

// Returns YES if a variation triggering server-side behavior is enabled.
+ (BOOL)isTriggerVariationEnabled:(int)variationID;

// Returns YES if UmaCellular feature is enabled.
+ (BOOL)isUMACellularEnabled WARN_UNUSED_RESULT;

// Returns YES if UKM feature is enabled.
+ (BOOL)isUKMEnabled WARN_UNUSED_RESULT;

// Returns YES if kTestFeature is enabled.
+ (BOOL)isTestFeatureEnabled;

// Returns YES if CreditCardScanner feature is enabled.
+ (BOOL)isCreditCardScannerEnabled WARN_UNUSED_RESULT;

// Returns YES if AutofillEnableCompanyName feature is enabled.
+ (BOOL)isAutofillCompanyNameEnabled WARN_UNUSED_RESULT;

// Returns YES if DemographicMetricsReporting feature is enabled.
+ (BOOL)isDemographicMetricsReportingEnabled WARN_UNUSED_RESULT;

// Returns YES if the |launchSwitch| is found in host app launch switches.
+ (BOOL)appHasLaunchSwitch:(NSString*)launchSwitch;

// Returns YES if custom WebKit frameworks were properly loaded, rather than
// system frameworks. Always returns YES if the app was not requested to run
// with custom WebKit frameworks.
+ (BOOL)isCustomWebKitLoadedIfRequested WARN_UNUSED_RESULT;

// Returns YES if collections are presented in cards.
+ (BOOL)isCollectionsCardPresentationStyleEnabled WARN_UNUSED_RESULT;

#pragma mark - Popup Blocking

// Gets the current value of the popup content setting preference for the
// original browser state.
+ (ContentSetting)popupPrefValue;

// Sets the popup content setting preference to the given value for the original
// browser state.
+ (void)setPopupPrefValue:(ContentSetting)value;

#pragma mark - Pref Utilities (EG2)

// Gets the value of a local state pref. Returns a
// base::Value encoded as a JSON string. If the pref was not registered,
// returns a Value of type NONE.
+ (NSString*)localStatePrefValue:(NSString*)prefName;

// Gets the value of a user pref in the original browser state. Returns a
// base::Value encoded as a JSON string. If the pref was not registered,
// returns a Value of type NONE.
+ (NSString*)userPrefValue:(NSString*)prefName;

// Sets the value of a boolean user pref in the original browser state.
+ (void)setBoolValue:(BOOL)value forUserPref:(NSString*)prefName;

// Resets the BrowsingDataPrefs, which defines if its selected or not when
// clearing Browsing data.
+ (void)resetBrowsingDataPrefs;

#pragma mark - Keyboard Command utilities

// The count of key commands registered with the currently active BVC.
+ (NSInteger)registeredKeyCommandCount;

// Simulates a physical keyboard event.
// The input is similar to UIKeyCommand parameters, and is designed for testing
// keyboard shortcuts.
// Accepts any strings and also UIKeyInput{Up|Down|Left|Right}Arrow and
// UIKeyInputEscape constants as |input|.
+ (void)simulatePhysicalKeyboardEvent:(NSString*)input
                                flags:(UIKeyModifierFlags)flags;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_
