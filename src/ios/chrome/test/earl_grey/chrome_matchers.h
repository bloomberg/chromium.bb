// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_

#import <Foundation/Foundation.h>

#include <string>

#import "ios/chrome/test/earl_grey/chrome_matchers_shorthand.h"

@protocol GREYMatcher;

@interface ChromeMatchers : NSObject

// Matcher for element with accessibility label corresponding to |label| and
// accessibility trait UIAccessibilityTraitButton.
+ (id<GREYMatcher>)buttonWithAccessibilityLabel:(NSString*)label;

// Matcher for element with accessibility label corresponding to |messageId|
// and accessibility trait UIAccessibilityTraitButton.
+ (id<GREYMatcher>)buttonWithAccessibilityLabelId:(int)messageId;

// Matcher for element with an image corresponding to |imageId|.
+ (id<GREYMatcher>)imageViewWithImage:(int)imageId;

// Matcher for element with an image defined by its name in the main bundle.
+ (id<GREYMatcher>)imageViewWithImageNamed:(NSString*)imageName;

// Matcher for element with an image corresponding to |imageId| and
// accessibility trait UIAccessibilityTraitButton.
+ (id<GREYMatcher>)buttonWithImage:(int)imageId;

// Matcher for element with accessibility label corresponding to |messageId|
// and accessibility trait UIAccessibilityTraitStaticText.
+ (id<GREYMatcher>)staticTextWithAccessibilityLabelId:(int)messageId;

// Matcher for element with accessibility label corresponding to |label| and
// accessibility trait UIAccessibilityTraitStaticText.
+ (id<GREYMatcher>)staticTextWithAccessibilityLabel:(NSString*)label;

// Matcher for element with accessibility label corresponding to |messageId|
// and accessibility trait UIAccessibilityTraitHeader.
+ (id<GREYMatcher>)headerWithAccessibilityLabelId:(int)messageId;

// Matcher for element with accessibility label corresponding to |label| and
// accessibility trait UIAccessibilityTraitHeader.
+ (id<GREYMatcher>)headerWithAccessibilityLabel:(NSString*)label;

// Returns matcher for a cancel button.
+ (id<GREYMatcher>)cancelButton;

// Returns matcher for a close button.
+ (id<GREYMatcher>)closeButton;

// Matcher for the navigate forward button.
+ (id<GREYMatcher>)forwardButton;

// Matcher for the navigate backward button.
+ (id<GREYMatcher>)backButton;

// Matcher for the reload button.
+ (id<GREYMatcher>)reloadButton;

// Matcher for the stop loading button.
+ (id<GREYMatcher>)stopButton;

// Returns a matcher for the omnibox.
+ (id<GREYMatcher>)omnibox;

// Returns a matcher for the location view.
+ (id<GREYMatcher>)defocusedLocationView;

// Returns a matcher for the page security info button.
+ (id<GREYMatcher>)pageSecurityInfoButton;

// Returns a matcher for the page security info indicator.
+ (id<GREYMatcher>)pageSecurityInfoIndicator;

// Returns matcher for omnibox containing |text|. Performs an exact match of the
// omnibox contents.
+ (id<GREYMatcher>)omniboxText:(std::string)text;

// Returns matcher for |text| being a substring of the text in the omnibox.
+ (id<GREYMatcher>)omniboxContainingText:(std::string)text;

// Returns matcher for |text| being a substring of the text in the location
// view.
+ (id<GREYMatcher>)locationViewContainingText:(std::string)text;

// Matcher for Tools menu button.
+ (id<GREYMatcher>)toolsMenuButton;

// Matcher for the Share menu button.
+ (id<GREYMatcher>)shareButton;

// Returns the GREYMatcher for the button that opens the tab switcher.
+ (id<GREYMatcher>)tabletTabSwitcherOpenButton;

// Matcher for show tabs button.
+ (id<GREYMatcher>)showTabsButton;

// Matcher for SettingsSwitchCell.
+ (id<GREYMatcher>)settingsSwitchCell:(NSString*)accessibilityIdentifier
                          isToggledOn:(BOOL)isToggledOn;

// Matcher for SettingsSwitchCell.
+ (id<GREYMatcher>)settingsSwitchCell:(NSString*)accessibilityIdentifier
                          isToggledOn:(BOOL)isToggledOn
                            isEnabled:(BOOL)isEnabled;

// Matcher for SyncSwitchCell.
+ (id<GREYMatcher>)syncSwitchCell:(NSString*)accessibilityLabel
                      isToggledOn:(BOOL)isToggledOn;

// Matcher for the Open in New Tab option in the context menu when long pressing
// a link.
+ (id<GREYMatcher>)openLinkInNewTabButton;

// Matcher for the done button on the navigation bar.
+ (id<GREYMatcher>)navigationBarDoneButton;

// Matcher for the done button on the Bookmarks navigation bar.
+ (id<GREYMatcher>)bookmarksNavigationBarDoneButton;

// Returns matcher for the account consistency setup signin button.
+ (id<GREYMatcher>)accountConsistencySetupSigninButton;

// Returns matcher for the account consistency confirmation button.
+ (id<GREYMatcher>)accountConsistencyConfirmationOkButton;

// Returns matcher for the add account accounts button.
+ (id<GREYMatcher>)addAccountButton;

// Returns matcher for the sign out accounts button.
+ (id<GREYMatcher>)signOutAccountsButton;

// Returns matcher for the Clear Browsing Data cell on the Privacy screen.
+ (id<GREYMatcher>)clearBrowsingDataCell;

// Returns matcher for the clear browsing data button on the clear browsing data
// panel.
+ (id<GREYMatcher>)clearBrowsingDataButton;

// Returns matcher for the clear browsing data collection view.
+ (id<GREYMatcher>)clearBrowsingDataCollectionView;

// Matcher for the clear browsing data action sheet item.
+ (id<GREYMatcher>)confirmClearBrowsingDataButton;

// Returns matcher for the settings button in the tools menu.
+ (id<GREYMatcher>)settingsMenuButton;

// Returns matcher for the "Done" button in the settings' navigation bar.
+ (id<GREYMatcher>)settingsDoneButton;

// Returns matcher for the tools menu table view.
+ (id<GREYMatcher>)toolsMenuView;

// Returns matcher for the OK button.
+ (id<GREYMatcher>)okButton;

// Returns matcher for the primary button in the sign-in promo view. This is
// "Sign in into Chrome" button for a cold state, or "Continue as John Doe" for
// a warm state.
+ (id<GREYMatcher>)primarySignInButton;

// Returns matcher for the secondary button in the sign-in promo view. This is
// "Not johndoe@example.com" button.
+ (id<GREYMatcher>)secondarySignInButton;

// Returns matcher for the button for the currently signed in account in the
// settings menu.
+ (id<GREYMatcher>)settingsAccountButton;

// Returns matcher for the accounts collection view.
+ (id<GREYMatcher>)settingsAccountsCollectionView;

// Returns matcher for the Import Data cell in switch sync account view.
+ (id<GREYMatcher>)settingsImportDataImportButton;

// Returns matcher for the Keep Data Separate cell in switch sync account view.
+ (id<GREYMatcher>)settingsImportDataKeepSeparateButton;

// Returns matcher for the Manage Synced Data button in sync setting view.
+ (id<GREYMatcher>)settingsSyncManageSyncedDataButton;

// Returns matcher for the menu button to sync accounts.
+ (id<GREYMatcher>)accountsSyncButton;

// Returns matcher for the Content Settings button on the main Settings screen.
+ (id<GREYMatcher>)contentSettingsButton;

// Returns matcher for the Google Services Settings button on the main Settings
// screen.
+ (id<GREYMatcher>)googleServicesSettingsButton;

// Returns matcher for the back button on a settings menu.
+ (id<GREYMatcher>)settingsMenuBackButton;

// Returns matcher for the Privacy cell on the main Settings screen.
+ (id<GREYMatcher>)settingsMenuPrivacyButton;

// Returns matcher for the Save passwords cell on the main Settings screen.
+ (id<GREYMatcher>)settingsMenuPasswordsButton;

// Returns matcher for the payment request collection view.
+ (id<GREYMatcher>)paymentRequestView;

// Returns matcher for the error confirmation view for payment request.
+ (id<GREYMatcher>)paymentRequestErrorView;

// Returns matcher for the voice search button on the main Settings screen.
+ (id<GREYMatcher>)voiceSearchButton;

// Returns matcher for the settings main menu view.
+ (id<GREYMatcher>)settingsCollectionView;

// Returns matcher for the clear browsing history cell on the clear browsing
// data panel.
+ (id<GREYMatcher>)clearBrowsingHistoryButton;

// Returns matcher for the clear cookies cell on the clear browsing data panel.
+ (id<GREYMatcher>)clearCookiesButton;

// Returns matcher for the clear cache cell on the clear browsing data panel.
+ (id<GREYMatcher>)clearCacheButton;

// Returns matcher for the clear saved passwords cell on the clear browsing data
// panel.
+ (id<GREYMatcher>)clearSavedPasswordsButton;

// Returns matcher for the collection view of content suggestion.
+ (id<GREYMatcher>)contentSuggestionCollectionView;

// Returns matcher for the warning message while filling in payment requests.
+ (id<GREYMatcher>)warningMessageView;

// Returns matcher for the payment picker cell.
+ (id<GREYMatcher>)paymentRequestPickerRow;

// Returns matcher for the payment request search bar.
+ (id<GREYMatcher>)paymentRequestPickerSearchBar;

// Returns matcher for the bookmarks button on the Tools menu.
+ (id<GREYMatcher>)bookmarksMenuButton;

// Returns matcher for the recent tabs button on the Tools menu.
+ (id<GREYMatcher>)recentTabsMenuButton;

// Returns matcher for the system selection callout.
+ (id<GREYMatcher>)systemSelectionCallout;

// Returns matcher for the copy button on the system selection callout.
+ (id<GREYMatcher>)systemSelectionCalloutCopyButton;

// Returns matcher for the Copy item on the context menu.
+ (id<GREYMatcher>)contextMenuCopyButton;

// Returns matcher for defoucesed omnibox on a new tab.
+ (id<GREYMatcher>)NTPOmnibox;

// Returns a matcher for the current WebView.
+ (id<GREYMatcher>)webViewMatcher;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_
