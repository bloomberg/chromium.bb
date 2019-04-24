// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <WebKit/WebKit.h>

#import "ios/chrome/test/earl_grey/chrome_matchers_shorthand.h"

#import "ios/chrome/test/earl_grey/chrome_matchers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

id<GREYMatcher> ButtonWithAccessibilityLabel(NSString* label) {
  return [ChromeMatchers buttonWithAccessibilityLabel:label];
}

id<GREYMatcher> ButtonWithAccessibilityLabelId(int message_id) {
  return [ChromeMatchers buttonWithAccessibilityLabelId:message_id];
}

id<GREYMatcher> ImageViewWithImageNamed(NSString* imageName) {
  return [ChromeMatchers imageViewWithImageNamed:imageName];
}

id<GREYMatcher> ImageViewWithImage(int image_id) {
  return [ChromeMatchers imageViewWithImage:image_id];
}

id<GREYMatcher> ButtonWithImage(int image_id) {
  return [ChromeMatchers buttonWithImage:image_id];
}

id<GREYMatcher> StaticTextWithAccessibilityLabelId(int message_id) {
  return [ChromeMatchers staticTextWithAccessibilityLabelId:message_id];
}

id<GREYMatcher> StaticTextWithAccessibilityLabel(NSString* label) {
  return [ChromeMatchers staticTextWithAccessibilityLabel:label];
}

id<GREYMatcher> HeaderWithAccessibilityLabelId(int message_id) {
  return [ChromeMatchers headerWithAccessibilityLabelId:message_id];
}

id<GREYMatcher> HeaderWithAccessibilityLabel(NSString* label) {
  return [ChromeMatchers headerWithAccessibilityLabel:label];
}

id<GREYMatcher> CancelButton() {
  return [ChromeMatchers cancelButton];
}

id<GREYMatcher> CloseButton() {
  return [ChromeMatchers closeButton];
}

id<GREYMatcher> ForwardButton() {
  return [ChromeMatchers forwardButton];
}

id<GREYMatcher> BackButton() {
  return [ChromeMatchers backButton];
}

id<GREYMatcher> ReloadButton() {
  return [ChromeMatchers reloadButton];
}

id<GREYMatcher> StopButton() {
  return [ChromeMatchers stopButton];
}

id<GREYMatcher> Omnibox() {
  return [ChromeMatchers omnibox];
}

id<GREYMatcher> DefocusedLocationView() {
  return [ChromeMatchers defocusedLocationView];
}

id<GREYMatcher> PageSecurityInfoButton() {
  return [ChromeMatchers pageSecurityInfoButton];
}

id<GREYMatcher> PageSecurityInfoIndicator() {
  return [ChromeMatchers pageSecurityInfoIndicator];
}

id<GREYMatcher> OmniboxText(std::string text) {
  return [ChromeMatchers omniboxText:text];
}

id<GREYMatcher> OmniboxContainingText(std::string text) {
  return [ChromeMatchers omniboxContainingText:text];
}

id<GREYMatcher> LocationViewContainingText(std::string text) {
  return [ChromeMatchers locationViewContainingText:text];
}

id<GREYMatcher> ToolsMenuButton() {
  return [ChromeMatchers toolsMenuButton];
}

id<GREYMatcher> ShareButton() {
  return [ChromeMatchers shareButton];
}

id<GREYMatcher> TabletTabSwitcherOpenButton() {
  return [ChromeMatchers tabletTabSwitcherOpenButton];
}

id<GREYMatcher> ShowTabsButton() {
  return [ChromeMatchers showTabsButton];
}

id<GREYMatcher> SettingsSwitchCell(NSString* accessibility_identifier,
                                   BOOL is_toggled_on) {
  return [ChromeMatchers settingsSwitchCell:accessibility_identifier
                                isToggledOn:is_toggled_on];
}

id<GREYMatcher> SettingsSwitchCell(NSString* accessibility_identifier,
                                   BOOL is_toggled_on,
                                   BOOL is_enabled) {
  return [ChromeMatchers settingsSwitchCell:accessibility_identifier
                                isToggledOn:is_toggled_on
                                  isEnabled:is_enabled];
}

id<GREYMatcher> SyncSwitchCell(NSString* accessibility_label,
                               BOOL is_toggled_on) {
  return [ChromeMatchers syncSwitchCell:accessibility_label
                            isToggledOn:is_toggled_on];
}

id<GREYMatcher> OpenLinkInNewTabButton() {
  return [ChromeMatchers openLinkInNewTabButton];
}

id<GREYMatcher> NavigationBarDoneButton() {
  return [ChromeMatchers navigationBarDoneButton];
}

id<GREYMatcher> BookmarksNavigationBarDoneButton() {
  return [ChromeMatchers bookmarksNavigationBarDoneButton];
}

id<GREYMatcher> AccountConsistencySetupSigninButton() {
  return [ChromeMatchers accountConsistencySetupSigninButton];
}

id<GREYMatcher> AccountConsistencyConfirmationOkButton() {
  return [ChromeMatchers accountConsistencyConfirmationOkButton];
}

id<GREYMatcher> AddAccountButton() {
  return [ChromeMatchers addAccountButton];
}

id<GREYMatcher> SignOutAccountsButton() {
  return [ChromeMatchers signOutAccountsButton];
}

id<GREYMatcher> ClearBrowsingDataCell() {
  return [ChromeMatchers clearBrowsingDataCell];
}

id<GREYMatcher> ClearBrowsingDataButton() {
  return [ChromeMatchers clearBrowsingDataButton];
}

id<GREYMatcher> ClearBrowsingDataCollectionView() {
  return [ChromeMatchers clearBrowsingDataCollectionView];
}

id<GREYMatcher> ConfirmClearBrowsingDataButton() {
  return [ChromeMatchers confirmClearBrowsingDataButton];
}

id<GREYMatcher> SettingsMenuButton() {
  return [ChromeMatchers settingsMenuButton];
}

id<GREYMatcher> SettingsDoneButton() {
  return [ChromeMatchers settingsDoneButton];
}

id<GREYMatcher> ToolsMenuView() {
  return [ChromeMatchers toolsMenuView];
}

id<GREYMatcher> OKButton() {
  return [ChromeMatchers okButton];
}

id<GREYMatcher> PrimarySignInButton() {
  return [ChromeMatchers primarySignInButton];
}

id<GREYMatcher> SecondarySignInButton() {
  return [ChromeMatchers secondarySignInButton];
}

id<GREYMatcher> SettingsAccountButton() {
  return [ChromeMatchers settingsAccountButton];
}

id<GREYMatcher> SettingsAccountsCollectionView() {
  return [ChromeMatchers settingsAccountsCollectionView];
}

id<GREYMatcher> SettingsImportDataImportButton() {
  return [ChromeMatchers settingsImportDataImportButton];
}

id<GREYMatcher> SettingsImportDataKeepSeparateButton() {
  return [ChromeMatchers settingsImportDataKeepSeparateButton];
}

id<GREYMatcher> SettingsSyncManageSyncedDataButton() {
  return [ChromeMatchers settingsSyncManageSyncedDataButton];
}

id<GREYMatcher> AccountsSyncButton() {
  return [ChromeMatchers accountsSyncButton];
}

id<GREYMatcher> ContentSettingsButton() {
  return [ChromeMatchers contentSettingsButton];
}

id<GREYMatcher> GoogleServicesSettingsButton() {
  return [ChromeMatchers googleServicesSettingsButton];
}

id<GREYMatcher> SettingsMenuBackButton() {
  return [ChromeMatchers settingsMenuBackButton];
}

id<GREYMatcher> SettingsMenuPrivacyButton() {
  return [ChromeMatchers settingsMenuPrivacyButton];
}

id<GREYMatcher> SettingsMenuPasswordsButton() {
  return [ChromeMatchers settingsMenuPasswordsButton];
}

id<GREYMatcher> PaymentRequestView() {
  return [ChromeMatchers paymentRequestView];
}

id<GREYMatcher> PaymentRequestErrorView() {
  return [ChromeMatchers paymentRequestErrorView];
}

id<GREYMatcher> VoiceSearchButton() {
  return [ChromeMatchers voiceSearchButton];
}

id<GREYMatcher> SettingsCollectionView() {
  return [ChromeMatchers settingsCollectionView];
}

id<GREYMatcher> ClearBrowsingHistoryButton() {
  return [ChromeMatchers clearBrowsingHistoryButton];
}

id<GREYMatcher> ClearCookiesButton() {
  return [ChromeMatchers clearCookiesButton];
}

id<GREYMatcher> ClearCacheButton() {
  return [ChromeMatchers clearCacheButton];
}

id<GREYMatcher> ClearSavedPasswordsButton() {
  return [ChromeMatchers clearSavedPasswordsButton];
}

id<GREYMatcher> ContentSuggestionCollectionView() {
  return [ChromeMatchers contentSuggestionCollectionView];
}

id<GREYMatcher> WarningMessageView() {
  return [ChromeMatchers warningMessageView];
}

id<GREYMatcher> PaymentRequestPickerRow() {
  return [ChromeMatchers paymentRequestPickerRow];
}

id<GREYMatcher> PaymentRequestPickerSearchBar() {
  return [ChromeMatchers paymentRequestPickerSearchBar];
}

id<GREYMatcher> BookmarksMenuButton() {
  return [ChromeMatchers bookmarksMenuButton];
}

id<GREYMatcher> RecentTabsMenuButton() {
  return [ChromeMatchers recentTabsMenuButton];
}

id<GREYMatcher> SystemSelectionCallout() {
  return [ChromeMatchers systemSelectionCallout];
}

id<GREYMatcher> SystemSelectionCalloutCopyButton() {
  return [ChromeMatchers systemSelectionCalloutCopyButton];
}

id<GREYMatcher> ContextMenuCopyButton() {
  return [ChromeMatchers contextMenuCopyButton];
}

id<GREYMatcher> NewTabPageOmnibox() {
  return [ChromeMatchers NTPOmnibox];
}

id<GREYMatcher> WebViewMatcher() {
  return [ChromeMatchers webViewMatcher];
}

}  // namespace chrome_test_util
