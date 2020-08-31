// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_APP_INTERFACE_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@protocol GREYMatcher;

// Helper class to return matchers for EG tests.  These helpers are compiled
// into the app binary and can be called from either app or test code.
// All calls of grey_... involve the App process, so it's more efficient to
// define the matchers in the app process.
@interface ChromeMatchersAppInterface : NSObject

// Matcher for element with accessibility label corresponding to |label| and
// accessibility trait UIAccessibilityTraitButton.
+ (id<GREYMatcher>)buttonWithAccessibilityLabel:(NSString*)label;

// Matcher for element with accessibility label corresponding to |messageID|
// and accessibility trait UIAccessibilityTraitButton.
+ (id<GREYMatcher>)buttonWithAccessibilityLabelID:(int)messageID;

// Matcher for element with an image corresponding to |imageID|.
+ (id<GREYMatcher>)imageViewWithImage:(int)imageID;

// Matcher for element with an image defined by its name in the main bundle.
+ (id<GREYMatcher>)imageViewWithImageNamed:(NSString*)imageName;

// Matcher for element with an image corresponding to |imageID| and
// accessibility trait UIAccessibilityTraitButton.
+ (id<GREYMatcher>)buttonWithImage:(int)imageID;

// Matcher for element with accessibility label corresponding to |messageID|
// and accessibility trait UIAccessibilityTraitStaticText.
+ (id<GREYMatcher>)staticTextWithAccessibilityLabelID:(int)messageID;

// Matcher for element with accessibility label corresponding to |label| and
// accessibility trait UIAccessibilityTraitStaticText.
+ (id<GREYMatcher>)staticTextWithAccessibilityLabel:(NSString*)label;

// Matcher for element with accessibility label corresponding to |messageID|
// and accessibility trait UIAccessibilityTraitHeader.
+ (id<GREYMatcher>)headerWithAccessibilityLabelID:(int)messageID;

// Matcher for text field of a cell with |messageID|.
+ (id<GREYMatcher>)textFieldForCellWithLabelID:(int)messageID;

// Matcher for icon view of a cell with |messageID|.
+ (id<GREYMatcher>)iconViewForCellWithLabelID:(int)messageID
                                     iconType:(NSString*)iconType;

// Matcher for element with accessibility label corresponding to |label| and
// accessibility trait UIAccessibilityTraitHeader.
+ (id<GREYMatcher>)headerWithAccessibilityLabel:(NSString*)label;

// Returns matcher for the primary toolbar.
+ (id<GREYMatcher>)primaryToolbar;

// Returns matcher for a cancel button.
+ (id<GREYMatcher>)cancelButton;

// Returns the matcher for an enabled cancel button in a navigation bar.
+ (id<GREYMatcher>)navigationBarCancelButton;

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
+ (id<GREYMatcher>)omniboxText:(NSString*)text;

// Returns matcher for |text| being a substring of the text in the omnibox.
+ (id<GREYMatcher>)omniboxContainingText:(NSString*)text;

// Returns matcher for |text| being a substring of the text in the location
// view.
+ (id<GREYMatcher>)locationViewContainingText:(NSString*)text;

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

// Matcher for the back button on the Bookmarks navigation bar.
+ (id<GREYMatcher>)bookmarksNavigationBarBackButton;

// Returns matcher for the account consistency confirmation button.
+ (id<GREYMatcher>)accountConsistencyConfirmationOKButton;

// Returns matcher for "ADD ACCOUNT" button in unified consent dialog.
+ (id<GREYMatcher>)unifiedConsentAddAccountButton;

// Returns matcher for the add account accounts button.
+ (id<GREYMatcher>)addAccountButton;

// Returns matcher for the sign out accounts button.
+ (id<GREYMatcher>)signOutAccountsButton;

// Returns matcher for the Clear Browsing Data cell on the Privacy screen.
+ (id<GREYMatcher>)clearBrowsingDataCell;

// Returns matcher for the clear browsing data button on the clear browsing data
// panel.
+ (id<GREYMatcher>)clearBrowsingDataButton;

// Returns matcher for the clear browsing data view.
+ (id<GREYMatcher>)clearBrowsingDataView;

// Matcher for the clear browsing data action sheet item.
+ (id<GREYMatcher>)confirmClearBrowsingDataButton;

// Returns matcher for the settings button in the tools menu.
+ (id<GREYMatcher>)settingsMenuButton;

// Returns matcher for the "Done" button in the settings' navigation bar.
+ (id<GREYMatcher>)settingsDoneButton;

// Returns matcher for the "Confirm" button in the Sync and Google services
// settings' navigation bar.
+ (id<GREYMatcher>)syncSettingsConfirmButton;

// Returns matcher for the Autofill Credit Card "Payment Methods" view in the
// settings menu.
+ (id<GREYMatcher>)autofillCreditCardTableView;

// Returns matcher for the "Payment Methods" button in the settings menu.
+ (id<GREYMatcher>)paymentMethodsButton;

// Returns matcher for the "Add Credit Card" view in the Settings menu.
+ (id<GREYMatcher>)addCreditCardView;

// Returns matcher for the "Add Payment Method" button in the Settings Payment
// Methods view.
+ (id<GREYMatcher>)addPaymentMethodButton;

// Returns matcher for the "Add" credit card button in the Payment
// Methods add credit card view.
+ (id<GREYMatcher>)addCreditCardButton;

// Returns matcher for the "Cancel" button in the Payment Methods add credit
// card view.
+ (id<GREYMatcher>)addCreditCardCancelButton;

// Returns matcher for the "Credit Card Scanner" view.
+ (id<GREYMatcher>)creditCardScannerView;

// Returns matcher for the tools menu table view.
+ (id<GREYMatcher>)toolsMenuView;

// Returns matcher for the OK button.
+ (id<GREYMatcher>)OKButton;

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

// Returns matcher for the Continue navigation button in switch sync account
// view.
+ (id<GREYMatcher>)settingsImportDataContinueButton;

// Returns matcher for the privacy table view.
+ (id<GREYMatcher>)settingsPrivacyTableView;

// Returns matcher for the menu button to sync accounts.
+ (id<GREYMatcher>)accountsSyncButton;

// Returns matcher for the Content Settings button on the main Settings screen.
+ (id<GREYMatcher>)contentSettingsButton;

// Returns matcher for the Google Services Settings button on the main Settings
// screen.
+ (id<GREYMatcher>)googleServicesSettingsButton;

// Returns matcher for the Google Services Settings view.
+ (id<GREYMatcher>)googleServicesSettingsView;

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

// Returns matcher for the voice search button on the omnibox input accessory.
+ (id<GREYMatcher>)voiceSearchInputAccessoryButton;

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

// Returns matcher for the clear saved passwords cell on the clear browsing data
// panel.
+ (id<GREYMatcher>)clearAutofillButton;

// Returns matcher for the collection view of content suggestion.
+ (id<GREYMatcher>)contentSuggestionCollectionView;

// Returns matcher for the warning message while filling in payment requests.
+ (id<GREYMatcher>)warningMessageView;

// Returns matcher for the payment picker cell.
+ (id<GREYMatcher>)paymentRequestPickerRow;

// Returns matcher for the payment request search bar.
+ (id<GREYMatcher>)paymentRequestPickerSearchBar;

// Returns matcher for the reading list button on the Tools menu.
+ (id<GREYMatcher>)readingListMenuButton;

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

// Returns matcher for a fake omnibox on a new tab page.
+ (id<GREYMatcher>)fakeOmnibox;

// Returns a matcher for the current WebView.
+ (id<GREYMatcher>)webViewMatcher;

// Returns a matcher for the current WebState's scroll view.
+ (id<GREYMatcher>)webStateScrollViewMatcher;

// Returns a matcher for the Clear Browsing Data button in the History UI.
+ (id<GREYMatcher>)historyClearBrowsingDataButton;

// Returns a matcher for "Open In..." button.
+ (id<GREYMatcher>)openInButton;

// Returns the GREYMatcher for the button that opens the tab grid.
+ (id<GREYMatcher>)tabGridOpenButton;

// Returns the GREYMatcher for the cell at |index| in the tab grid.
+ (id<GREYMatcher>)tabGridCellAtIndex:(unsigned int)index;

// Returns the GREYMatcher for the button that closes the tab grid.
+ (id<GREYMatcher>)tabGridDoneButton;

// Returns the GREYMatcher for the button that closes all the tabs in the tab
// grid.
+ (id<GREYMatcher>)tabGridCloseAllButton;

// Returns the GREYMatcher for the button that reverts the close all tabs action
// in the tab grid.
+ (id<GREYMatcher>)tabGridUndoCloseAllButton;

// Returns the GREYMatcher for the cell that opens History in Recent Tabs.
+ (id<GREYMatcher>)tabGridSelectShowHistoryCell;

// Returns the GREYMatcher for the regular tabs empty state view.
+ (id<GREYMatcher>)tabGridRegularTabsEmptyStateView;

// Returns the GREYMatcher for the button that creates new non incognito tabs
// from within the tab grid.
+ (id<GREYMatcher>)tabGridNewTabButton;

// Returns the GREYMatcher for the button that creates new incognito tabs from
// within the tab grid.
+ (id<GREYMatcher>)tabGridNewIncognitoTabButton;

// Returns the GREYMatcher for the button to go to the non incognito panel in
// the tab grid.
+ (id<GREYMatcher>)tabGridOpenTabsPanelButton;

// Returns the GREYMatcher for the button to go to the incognito panel in
// the tab grid.
+ (id<GREYMatcher>)tabGridIncognitoTabsPanelButton;

// Returns the GREYMatcher for the button to go to the other devices panel in
// the tab grid.
+ (id<GREYMatcher>)tabGridOtherDevicesPanelButton;

// Returns the GREYMatcher for the button to close the cell at |index| in the
// tab grid.
+ (id<GREYMatcher>)tabGridCloseButtonForCellAtIndex:(unsigned int)index;

// Returns a matcher for the password settings collection view.
+ (id<GREYMatcher>)settingsPasswordMatcher;

// Returns a matcher for the search bar in password settings.
+ (id<GREYMatcher>)settingsPasswordSearchMatcher;

// Returns a matcher for the profiles settings collection view.
+ (id<GREYMatcher>)settingsProfileMatcher;

// Returns a matcher for the credit card settings collection view.
+ (id<GREYMatcher>)settingsCreditCardMatcher;

// Returns a matcher for the delete button at the bottom of settings collection
// views.
+ (id<GREYMatcher>)settingsBottomToolbarDeleteButton;

// Returns a matcher for the search engine button in the main settings view.
+ (id<GREYMatcher>)settingsSearchEngineButton;

// Returns a matcher for an autofill suggestion view.
+ (id<GREYMatcher>)autofillSuggestionViewMatcher;

// Returns a matcher to test whether the element is a scroll view with a content
// smaller than the scroll view bounds.
+ (id<GREYMatcher>)contentViewSmallerThanScrollView;

// Returns a matcher for the infobar asking to save a credit card locally.
+ (id<GREYMatcher>)autofillSaveCardLocallyInfobar;

// Returns a matcher for the infobar asking to upload a credit card.
+ (id<GREYMatcher>)autofillUploadCardInfobar;

// Returns a matcher for a history entry with |url| and |title|.
+ (id<GREYMatcher>)historyEntryForURL:(NSString*)URL title:(NSString*)title;

#pragma mark - Manual Fallback

// Returns a matcher for the scroll view in keyboard accessory bar.
+ (id<GREYMatcher>)manualFallbackFormSuggestionViewMatcher;

// Returns a matcher for the keyboard icon in the keyboard accessory bar.
+ (id<GREYMatcher>)manualFallbackKeyboardIconMatcher;

// Returns a matcher for the password icon in the keyboard accessory bar.
+ (id<GREYMatcher>)manualFallbackPasswordIconMatcher;

// Returns a matcher for the password table view in manual fallback.
+ (id<GREYMatcher>)manualFallbackPasswordTableViewMatcher;

// Returns a matcher for the password search bar in manual fallback.
+ (id<GREYMatcher>)manualFallbackPasswordSearchBarMatcher;

// Returns a matcher for the button to open password settings in manual
// fallback.
+ (id<GREYMatcher>)manualFallbackManagePasswordsMatcher;

// Returns a matcher for the button to open all passwords in manual fallback.
+ (id<GREYMatcher>)manualFallbackOtherPasswordsMatcher;

// Returns a matcher for the button to dismiss all passwords in manual fallback.
+ (id<GREYMatcher>)manualFallbackOtherPasswordsDismissMatcher;

// Returns a matcher for the a password in the manual fallback list.
+ (id<GREYMatcher>)manualFallbackPasswordButtonMatcher;

// Returns a matcher for the PasswordTableView window.
+ (id<GREYMatcher>)manualFallbackPasswordTableViewWindowMatcher;

// Returns a matcher for the profiles icon in the keyboard accessory bar.
+ (id<GREYMatcher>)manualFallbackProfilesIconMatcher;

// Returns a matcher for the profiles table view in manual fallback.
+ (id<GREYMatcher>)manualFallbackProfilesTableViewMatcher;
// Returns a matcher for the button to open profile settings in manual
// fallback.
+ (id<GREYMatcher>)manualFallbackManageProfilesMatcher;

// Returns a matcher for the ProfileTableView window.
+ (id<GREYMatcher>)manualFallbackProfileTableViewWindowMatcher;

// Returns a matcher for the credit card icon in the keyboard accessory bar.
+ (id<GREYMatcher>)manualFallbackCreditCardIconMatcher;

// Returns a matcher for the credit card table view in manual fallback.
+ (id<GREYMatcher>)manualFallbackCreditCardTableViewMatcher;

// Returns a matcher for the button to open password settings in manual
// fallback.
+ (id<GREYMatcher>)manualFallbackManageCreditCardsMatcher;

// Returns a matcher for the button to add credit cards settings in manual
// fallback.
+ (id<GREYMatcher>)manualFallbackAddCreditCardsMatcher;

// Returns a matcher for the CreditCardTableView window.
+ (id<GREYMatcher>)manualFallbackCreditCardTableViewWindowMatcher;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_APP_INTERFACE_H_
