// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_matchers.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(ChromeMatchersAppInterface)
#endif

namespace chrome_test_util {

id<GREYMatcher> ButtonWithAccessibilityLabel(NSString* label) {
  return [ChromeMatchersAppInterface buttonWithAccessibilityLabel:label];
}

id<GREYMatcher> ButtonWithAccessibilityLabelId(int message_id) {
  return [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:message_id];
}

id<GREYMatcher> ImageViewWithImageNamed(NSString* imageName) {
  return [ChromeMatchersAppInterface imageViewWithImageNamed:imageName];
}

id<GREYMatcher> ImageViewWithImage(int image_id) {
  return [ChromeMatchersAppInterface imageViewWithImage:image_id];
}

id<GREYMatcher> ButtonWithImage(int image_id) {
  return [ChromeMatchersAppInterface buttonWithImage:image_id];
}

id<GREYMatcher> StaticTextWithAccessibilityLabelId(int message_id) {
  return [ChromeMatchersAppInterface
      staticTextWithAccessibilityLabelID:message_id];
}

id<GREYMatcher> StaticTextWithAccessibilityLabel(NSString* label) {
  return [ChromeMatchersAppInterface staticTextWithAccessibilityLabel:label];
}

id<GREYMatcher> HeaderWithAccessibilityLabelId(int message_id) {
  return [ChromeMatchersAppInterface headerWithAccessibilityLabelID:message_id];
}

id<GREYMatcher> HeaderWithAccessibilityLabel(NSString* label) {
  return [ChromeMatchersAppInterface headerWithAccessibilityLabel:label];
}

id<GREYMatcher> TextFieldForCellWithLabelId(int message_id) {
  return [ChromeMatchersAppInterface textFieldForCellWithLabelID:message_id];
}

id<GREYMatcher> IconViewForCellWithLabelId(int message_id,
                                           NSString* icon_type) {
  return [ChromeMatchersAppInterface iconViewForCellWithLabelID:message_id
                                                       iconType:icon_type];
}

id<GREYMatcher> PrimaryToolbar() {
  return [ChromeMatchersAppInterface primaryToolbar];
}

id<GREYMatcher> CancelButton() {
  return [ChromeMatchersAppInterface cancelButton];
}

id<GREYMatcher> NavigationBarCancelButton() {
  return [ChromeMatchersAppInterface navigationBarCancelButton];
}

id<GREYMatcher> CloseButton() {
  return [ChromeMatchersAppInterface closeButton];
}

id<GREYMatcher> ForwardButton() {
  return [ChromeMatchersAppInterface forwardButton];
}

id<GREYMatcher> BackButton() {
  return [ChromeMatchersAppInterface backButton];
}

id<GREYMatcher> ReloadButton() {
  return [ChromeMatchersAppInterface reloadButton];
}

id<GREYMatcher> StopButton() {
  return [ChromeMatchersAppInterface stopButton];
}

id<GREYMatcher> Omnibox() {
  return [ChromeMatchersAppInterface omnibox];
}

id<GREYMatcher> DefocusedLocationView() {
  return [ChromeMatchersAppInterface defocusedLocationView];
}

id<GREYMatcher> PageSecurityInfoButton() {
  return [ChromeMatchersAppInterface pageSecurityInfoButton];
}

id<GREYMatcher> PageSecurityInfoIndicator() {
  return [ChromeMatchersAppInterface pageSecurityInfoIndicator];
}

id<GREYMatcher> OmniboxText(const std::string& text) {
  return [ChromeMatchersAppInterface omniboxText:base::SysUTF8ToNSString(text)];
}

id<GREYMatcher> OmniboxContainingText(const std::string& text) {
  return [ChromeMatchersAppInterface
      omniboxContainingText:base::SysUTF8ToNSString(text)];
}

id<GREYMatcher> LocationViewContainingText(const std::string& text) {
  return [ChromeMatchersAppInterface
      locationViewContainingText:base::SysUTF8ToNSString(text)];
}

id<GREYMatcher> ToolsMenuButton() {
  return [ChromeMatchersAppInterface toolsMenuButton];
}

id<GREYMatcher> ShareButton() {
  return [ChromeMatchersAppInterface shareButton];
}

id<GREYMatcher> TabletTabSwitcherOpenButton() {
  return [ChromeMatchersAppInterface tabletTabSwitcherOpenButton];
}

id<GREYMatcher> ShowTabsButton() {
  return [ChromeMatchersAppInterface showTabsButton];
}

id<GREYMatcher> SettingsSwitchCell(NSString* accessibility_identifier,
                                   BOOL is_toggled_on) {
  return [ChromeMatchersAppInterface settingsSwitchCell:accessibility_identifier
                                            isToggledOn:is_toggled_on];
}

id<GREYMatcher> SettingsSwitchCell(NSString* accessibility_identifier,
                                   BOOL is_toggled_on,
                                   BOOL is_enabled) {
  return [ChromeMatchersAppInterface settingsSwitchCell:accessibility_identifier
                                            isToggledOn:is_toggled_on
                                              isEnabled:is_enabled];
}

id<GREYMatcher> SyncSwitchCell(NSString* accessibility_label,
                               BOOL is_toggled_on) {
  return [ChromeMatchersAppInterface syncSwitchCell:accessibility_label
                                        isToggledOn:is_toggled_on];
}

id<GREYMatcher> OpenLinkInNewTabButton() {
  return [ChromeMatchersAppInterface openLinkInNewTabButton];
}

id<GREYMatcher> NavigationBarDoneButton() {
  return [ChromeMatchersAppInterface navigationBarDoneButton];
}

id<GREYMatcher> BookmarksNavigationBarDoneButton() {
  return [ChromeMatchersAppInterface bookmarksNavigationBarDoneButton];
}

id<GREYMatcher> BookmarksNavigationBarBackButton() {
  return [ChromeMatchersAppInterface bookmarksNavigationBarBackButton];
}

id<GREYMatcher> AccountConsistencyConfirmationOkButton() {
  return [ChromeMatchersAppInterface accountConsistencyConfirmationOKButton];
}

id<GREYMatcher> UnifiedConsentAddAccountButton() {
  return [ChromeMatchersAppInterface unifiedConsentAddAccountButton];
}

id<GREYMatcher> AddAccountButton() {
  return [ChromeMatchersAppInterface addAccountButton];
}

id<GREYMatcher> SignOutAccountsButton() {
  return [ChromeMatchersAppInterface signOutAccountsButton];
}

id<GREYMatcher> ClearBrowsingDataCell() {
  return [ChromeMatchersAppInterface clearBrowsingDataCell];
}

id<GREYMatcher> ClearBrowsingDataButton() {
  return [ChromeMatchersAppInterface clearBrowsingDataButton];
}

id<GREYMatcher> ClearBrowsingDataView() {
  return [ChromeMatchersAppInterface clearBrowsingDataView];
}

id<GREYMatcher> ConfirmClearBrowsingDataButton() {
  return [ChromeMatchersAppInterface confirmClearBrowsingDataButton];
}

id<GREYMatcher> SettingsMenuButton() {
  return [ChromeMatchersAppInterface settingsMenuButton];
}

id<GREYMatcher> SettingsDoneButton() {
  return [ChromeMatchersAppInterface settingsDoneButton];
}

id<GREYMatcher> SyncSettingsConfirmButton() {
  return [ChromeMatchersAppInterface syncSettingsConfirmButton];
}

id<GREYMatcher> AutofillCreditCardTableView() {
  return [ChromeMatchersAppInterface autofillCreditCardTableView];
}

id<GREYMatcher> PaymentMethodsButton() {
  return [ChromeMatchersAppInterface paymentMethodsButton];
}

id<GREYMatcher> AddCreditCardView() {
  return [ChromeMatchersAppInterface addCreditCardView];
}

id<GREYMatcher> AddPaymentMethodButton() {
  return [ChromeMatchersAppInterface addPaymentMethodButton];
}

id<GREYMatcher> AddCreditCardButton() {
  return [ChromeMatchersAppInterface addCreditCardButton];
}

id<GREYMatcher> AddCreditCardCancelButton() {
  return [ChromeMatchersAppInterface addCreditCardCancelButton];
}

id<GREYMatcher> CreditCardScannerView() {
  return [ChromeMatchersAppInterface creditCardScannerView];
}

id<GREYMatcher> ToolsMenuView() {
  return [ChromeMatchersAppInterface toolsMenuView];
}

id<GREYMatcher> OKButton() {
  return [ChromeMatchersAppInterface OKButton];
}

id<GREYMatcher> PrimarySignInButton() {
  return [ChromeMatchersAppInterface primarySignInButton];
}

id<GREYMatcher> SecondarySignInButton() {
  return [ChromeMatchersAppInterface secondarySignInButton];
}

id<GREYMatcher> SettingsAccountButton() {
  return [ChromeMatchersAppInterface settingsAccountButton];
}

id<GREYMatcher> SettingsAccountsCollectionView() {
  return [ChromeMatchersAppInterface settingsAccountsCollectionView];
}

id<GREYMatcher> SettingsImportDataImportButton() {
  return [ChromeMatchersAppInterface settingsImportDataImportButton];
}

id<GREYMatcher> SettingsImportDataKeepSeparateButton() {
  return [ChromeMatchersAppInterface settingsImportDataKeepSeparateButton];
}

id<GREYMatcher> SettingsImportDataContinueButton() {
  return [ChromeMatchersAppInterface settingsImportDataContinueButton];
}

id<GREYMatcher> SettingsPrivacyTableView() {
  return [ChromeMatchersAppInterface settingsPrivacyTableView];
}

id<GREYMatcher> AccountsSyncButton() {
  return [ChromeMatchersAppInterface accountsSyncButton];
}

id<GREYMatcher> ContentSettingsButton() {
  return [ChromeMatchersAppInterface contentSettingsButton];
}

id<GREYMatcher> GoogleServicesSettingsButton() {
  return [ChromeMatchersAppInterface googleServicesSettingsButton];
}

id<GREYMatcher> GoogleServicesSettingsView() {
  return [ChromeMatchersAppInterface googleServicesSettingsView];
}

id<GREYMatcher> SettingsMenuBackButton() {
  return [ChromeMatchersAppInterface settingsMenuBackButton];
}

id<GREYMatcher> SettingsMenuPrivacyButton() {
  return [ChromeMatchersAppInterface settingsMenuPrivacyButton];
}

id<GREYMatcher> SettingsMenuPasswordsButton() {
  return [ChromeMatchersAppInterface settingsMenuPasswordsButton];
}

id<GREYMatcher> PaymentRequestView() {
  return [ChromeMatchersAppInterface paymentRequestView];
}

id<GREYMatcher> PaymentRequestErrorView() {
  return [ChromeMatchersAppInterface paymentRequestErrorView];
}

id<GREYMatcher> VoiceSearchButton() {
  return [ChromeMatchersAppInterface voiceSearchButton];
}

id<GREYMatcher> VoiceSearchInputAccessoryButton() {
  return [ChromeMatchersAppInterface voiceSearchInputAccessoryButton];
}

id<GREYMatcher> SettingsCollectionView() {
  return [ChromeMatchersAppInterface settingsCollectionView];
}

id<GREYMatcher> ClearBrowsingHistoryButton() {
  return [ChromeMatchersAppInterface clearBrowsingHistoryButton];
}

id<GREYMatcher> ClearCookiesButton() {
  return [ChromeMatchersAppInterface clearCookiesButton];
}

id<GREYMatcher> ClearCacheButton() {
  return [ChromeMatchersAppInterface clearCacheButton];
}

id<GREYMatcher> ClearSavedPasswordsButton() {
  return [ChromeMatchersAppInterface clearSavedPasswordsButton];
}

id<GREYMatcher> ClearAutofillButton() {
  return [ChromeMatchersAppInterface clearAutofillButton];
}

id<GREYMatcher> ContentSuggestionCollectionView() {
  return [ChromeMatchersAppInterface contentSuggestionCollectionView];
}

id<GREYMatcher> WarningMessageView() {
  return [ChromeMatchersAppInterface warningMessageView];
}

id<GREYMatcher> PaymentRequestPickerRow() {
  return [ChromeMatchersAppInterface paymentRequestPickerRow];
}

id<GREYMatcher> PaymentRequestPickerSearchBar() {
  return [ChromeMatchersAppInterface paymentRequestPickerSearchBar];
}

id<GREYMatcher> ReadingListMenuButton() {
  return [ChromeMatchersAppInterface readingListMenuButton];
}

id<GREYMatcher> BookmarksMenuButton() {
  return [ChromeMatchersAppInterface bookmarksMenuButton];
}

id<GREYMatcher> RecentTabsMenuButton() {
  return [ChromeMatchersAppInterface recentTabsMenuButton];
}

id<GREYMatcher> SystemSelectionCallout() {
  return [ChromeMatchersAppInterface systemSelectionCallout];
}

id<GREYMatcher> SystemSelectionCalloutCopyButton() {
  return [ChromeMatchersAppInterface systemSelectionCalloutCopyButton];
}

id<GREYMatcher> ContextMenuCopyButton() {
  return [ChromeMatchersAppInterface contextMenuCopyButton];
}

id<GREYMatcher> NewTabPageOmnibox() {
  return [ChromeMatchersAppInterface NTPOmnibox];
}

id<GREYMatcher> FakeOmnibox() {
  return [ChromeMatchersAppInterface fakeOmnibox];
}

id<GREYMatcher> WebViewMatcher() {
  return [ChromeMatchersAppInterface webViewMatcher];
}

id<GREYMatcher> WebStateScrollViewMatcher() {
  return [ChromeMatchersAppInterface webStateScrollViewMatcher];
}

id<GREYMatcher> HistoryClearBrowsingDataButton() {
  return [ChromeMatchersAppInterface historyClearBrowsingDataButton];
}

id<GREYMatcher> OpenInButton() {
  return [ChromeMatchersAppInterface openInButton];
}

id<GREYMatcher> TabGridOpenButton() {
  return [ChromeMatchersAppInterface tabGridOpenButton];
}

id<GREYMatcher> TabGridCellAtIndex(unsigned int index) {
  return [ChromeMatchersAppInterface tabGridCellAtIndex:index];
}

id<GREYMatcher> TabGridDoneButton() {
  return [ChromeMatchersAppInterface tabGridDoneButton];
}

id<GREYMatcher> TabGridCloseAllButton() {
  return [ChromeMatchersAppInterface tabGridCloseAllButton];
}

id<GREYMatcher> TabGridUndoCloseAllButton() {
  return [ChromeMatchersAppInterface tabGridUndoCloseAllButton];
}

id<GREYMatcher> TabGridSelectShowHistoryCell() {
  return [ChromeMatchersAppInterface tabGridSelectShowHistoryCell];
}

id<GREYMatcher> TabGridRegularTabsEmptyStateView() {
  return [ChromeMatchersAppInterface tabGridRegularTabsEmptyStateView];
}

id<GREYMatcher> TabGridNewTabButton() {
  return [ChromeMatchersAppInterface tabGridNewTabButton];
}

id<GREYMatcher> TabGridNewIncognitoTabButton() {
  return [ChromeMatchersAppInterface tabGridNewIncognitoTabButton];
}

id<GREYMatcher> TabGridOpenTabsPanelButton() {
  return [ChromeMatchersAppInterface tabGridOpenTabsPanelButton];
}

id<GREYMatcher> TabGridIncognitoTabsPanelButton() {
  return [ChromeMatchersAppInterface tabGridIncognitoTabsPanelButton];
}

id<GREYMatcher> TabGridOtherDevicesPanelButton() {
  return [ChromeMatchersAppInterface tabGridOtherDevicesPanelButton];
}

id<GREYMatcher> TabGridCloseButtonForCellAtIndex(unsigned int index) {
  return [ChromeMatchersAppInterface tabGridCloseButtonForCellAtIndex:index];
}

id<GREYMatcher> SettingsPasswordMatcher() {
  return [ChromeMatchersAppInterface settingsPasswordMatcher];
}

id<GREYMatcher> SettingsPasswordSearchMatcher() {
  return [ChromeMatchersAppInterface settingsPasswordSearchMatcher];
}

id<GREYMatcher> SettingsProfileMatcher() {
  return [ChromeMatchersAppInterface settingsProfileMatcher];
}

id<GREYMatcher> SettingsCreditCardMatcher() {
  return [ChromeMatchersAppInterface settingsCreditCardMatcher];
}

id<GREYMatcher> SettingsBottomToolbarDeleteButton() {
  return [ChromeMatchersAppInterface settingsBottomToolbarDeleteButton];
}

id<GREYMatcher> SettingsSearchEngineButton() {
  return [ChromeMatchersAppInterface settingsSearchEngineButton];
}

id<GREYMatcher> AutofillSuggestionViewMatcher() {
  return [ChromeMatchersAppInterface autofillSuggestionViewMatcher];
}

id<GREYMatcher> ContentViewSmallerThanScrollView() {
  return [ChromeMatchersAppInterface contentViewSmallerThanScrollView];
}

id<GREYMatcher> AutofillSaveCardLocallyInfobar() {
  return [ChromeMatchersAppInterface autofillSaveCardLocallyInfobar];
}

id<GREYMatcher> AutofillUploadCardInfobar() {
  return [ChromeMatchersAppInterface autofillUploadCardInfobar];
}

id<GREYMatcher> HistoryEntry(const std::string& url, const std::string& title) {
  return [ChromeMatchersAppInterface
      historyEntryForURL:base::SysUTF8ToNSString(url)
                   title:base::SysUTF8ToNSString(title)];
}

#pragma mark - Manual Fallback

id<GREYMatcher> ManualFallbackFormSuggestionViewMatcher() {
  return [ChromeMatchersAppInterface manualFallbackFormSuggestionViewMatcher];
}

id<GREYMatcher> ManualFallbackKeyboardIconMatcher() {
  return [ChromeMatchersAppInterface manualFallbackKeyboardIconMatcher];
}

id<GREYMatcher> ManualFallbackPasswordIconMatcher() {
  return [ChromeMatchersAppInterface manualFallbackPasswordIconMatcher];
}

id<GREYMatcher> ManualFallbackPasswordTableViewMatcher() {
  return [ChromeMatchersAppInterface manualFallbackPasswordTableViewMatcher];
}

id<GREYMatcher> ManualFallbackPasswordSearchBarMatcher() {
  return [ChromeMatchersAppInterface manualFallbackPasswordSearchBarMatcher];
}

id<GREYMatcher> ManualFallbackManagePasswordsMatcher() {
  return [ChromeMatchersAppInterface manualFallbackManagePasswordsMatcher];
}

id<GREYMatcher> ManualFallbackOtherPasswordsMatcher() {
  return [ChromeMatchersAppInterface manualFallbackOtherPasswordsMatcher];
}

id<GREYMatcher> ManualFallbackOtherPasswordsDismissMatcher() {
  return
      [ChromeMatchersAppInterface manualFallbackOtherPasswordsDismissMatcher];
}

id<GREYMatcher> ManualFallbackPasswordButtonMatcher() {
  return [ChromeMatchersAppInterface manualFallbackPasswordButtonMatcher];
}

id<GREYMatcher> ManualFallbackPasswordTableViewWindowMatcher() {
  return
      [ChromeMatchersAppInterface manualFallbackPasswordTableViewWindowMatcher];
}

id<GREYMatcher> ManualFallbackProfilesIconMatcher() {
  return [ChromeMatchersAppInterface manualFallbackProfilesIconMatcher];
}

id<GREYMatcher> ManualFallbackProfilesTableViewMatcher() {
  return [ChromeMatchersAppInterface manualFallbackProfilesTableViewMatcher];
}

id<GREYMatcher> ManualFallbackManageProfilesMatcher() {
  return [ChromeMatchersAppInterface manualFallbackManageProfilesMatcher];
}

id<GREYMatcher> ManualFallbackProfileTableViewWindowMatcher() {
  return
      [ChromeMatchersAppInterface manualFallbackProfileTableViewWindowMatcher];
}

id<GREYMatcher> ManualFallbackCreditCardIconMatcher() {
  return [ChromeMatchersAppInterface manualFallbackCreditCardIconMatcher];
}

id<GREYMatcher> ManualFallbackCreditCardTableViewMatcher() {
  return [ChromeMatchersAppInterface manualFallbackCreditCardTableViewMatcher];
}

// Returns a matcher for the button to open password settings in manual
id<GREYMatcher> ManualFallbackManageCreditCardsMatcher() {
  return [ChromeMatchersAppInterface manualFallbackManageCreditCardsMatcher];
}

// Returns a matcher for the button to add credit cards settings in manual
id<GREYMatcher> ManualFallbackAddCreditCardsMatcher() {
  return [ChromeMatchersAppInterface manualFallbackAddCreditCardsMatcher];
}

id<GREYMatcher> ManualFallbackCreditCardTableViewWindowMatcher() {
  return [ChromeMatchersAppInterface
      manualFallbackCreditCardTableViewWindowMatcher];
}

}  // namespace chrome_test_util
