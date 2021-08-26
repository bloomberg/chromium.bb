// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/testing/earl_grey/earl_grey_test.h"

#include "base/json/json_string_value_serializer.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/history/core/common/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/translate/translate_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#include "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#include "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/browser/ui/settings/elements/elements_constants.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_ui_constants.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#include "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/chrome/test/earl_grey/test_switches.h"
#include "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns a JSON-encoded string representing the given |base::Value|. If
// |value| is nullptr, returns a string representing a |base::Value| of type
// NONE.
NSString* SerializeValue(const base::Value value) {
  std::string serialized_value;
  JSONStringValueSerializer serializer(&serialized_value);
  serializer.Serialize(std::move(value));
  return base::SysUTF8ToNSString(serialized_value);
}

// Sets the value of the policy with the |policy_key| key to the given value.
// The value must be serialized as a JSON string.
// Prefer using the other type-specific helpers instead of this generic helper
// if possible.
void SetPolicy(NSString* json_value, const std::string& policy_key) {
  [PolicyAppInterface setPolicyValue:json_value
                              forKey:base::SysUTF8ToNSString(policy_key)];
}

// Sets the value of the policy with the |policy_key| key to the given value.
// The value must be wrapped in a |base::Value|.
// Prefer using the other type-specific helpers instead of this generic helper
// if possible.
void SetPolicy(base::Value value, const std::string& policy_key) {
  SetPolicy(SerializeValue(std::move(value)), policy_key);
}

// Sets the value of the policy with the |policy_key| key to the given boolean
// value.
void SetPolicy(bool enabled, const std::string& policy_key) {
  SetPolicy(base::Value(enabled), policy_key);
}

// Sets the value of the policy with the |policy_key| key to the given integer
// value.
void SetPolicy(int value, const std::string& policy_key) {
  SetPolicy(base::Value(value), policy_key);
}

// TODO(crbug.com/1065522): Add helpers as needed for:
//    - STRING
//    - LIST (and subtypes, e.g. int list, string list, etc)
//    - DICTIONARY (and subtypes, e.g. int dictionary, string dictionary, etc)
//    - Deleting a policy value
//    - Setting multiple policies at once

// Verifies that a bool type policy sets the pref properly.
void VerifyBoolPolicy(const std::string& policy_key,
                      const std::string& pref_name) {
  // Loading chrome://policy isn't necessary for the test to succeed, but it
  // provides some visual feedback as the test runs.
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_POLICY_HEADER_NAME)];
  // Force the preference off via policy.
  SetPolicy(false, policy_key);
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:pref_name],
                  @"Preference was unexpectedly true");

  // Force the preference on via policy.
  SetPolicy(true, policy_key);
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:pref_name],
                 @"Preference was unexpectedly false");
}

// Returns a matcher for the Translate manual trigger button in the tools menu.
id<GREYMatcher> ToolsMenuTranslateButton() {
  return grey_allOf(grey_accessibilityID(kToolsMenuTranslateId),
                    grey_interactable(), nil);
}

// Verifies that a managed setting item is shown and react properly.
void VerifyManagedSettingItem(NSString* accessibilityID,
                              NSString* containerViewAccessibilityID) {
  // Check if the managed item is shown in the corresponding table view.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(accessibilityID),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(containerViewAccessibilityID)]
      assertWithMatcher:grey_notNil()];

  // Click the info button.
  [ChromeEarlGreyUI tapSettingsMenuButton:grey_accessibilityID(
                                              kTableViewCellInfoButtonViewId)];

  // Check if the contextual bubble is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap outside of the bubble.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewCellInfoButtonViewId)]
      performAction:grey_tap()];

  // Check if the contextual bubble is hidden.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_notVisible()];
}

}  // namespace

// Test case to verify that enterprise policies are set and respected.
@interface PolicyTestCase : ChromeTestCase
@end

@implementation PolicyTestCase {
  BOOL _settingsOpened;
}

- (void)tearDown {
  if (_settingsOpened) {
    [ChromeEarlGrey dismissSettings];
    [ChromeEarlGreyUI waitForAppToIdle];
  }
  [PolicyAppInterface clearPolicies];
  [super tearDown];
}

- (void)openSettingsMenu {
  [ChromeEarlGreyUI openSettingsMenu];
  _settingsOpened = YES;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  // Use commandline args to insert fake policy data into NSUserDefaults. To the
  // app, this policy data will appear under the
  // "com.apple.configuration.managed" key.
  AppLaunchConfiguration config;
  config.additional_args.push_back(std::string("--") +
                                   switches::kEnableEnterprisePolicy);
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

// Tests that about:policy is available.
- (void)testAboutPolicy {
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_POLICY_HEADER_NAME)];
}

// Tests changing the DefaultSearchProviderEnabled policy while the settings
// are open updates the UI.
- (void)testDefaultSearchProviderUpdate {
  SetPolicy(true, policy::key::kDefaultSearchProviderEnabled);

  [self openSettingsMenu];

  // Check that the non-managed item is present.
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                           kSettingsSearchEngineCellId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_allOf(
                               grey_accessibilityID(kSettingsTableViewId),
                               grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  SetPolicy(false, policy::key::kDefaultSearchProviderEnabled);

  // After setting the policy to false, the item should be replaced.
  VerifyManagedSettingItem(kSettingsManagedSearchEngineCellId,
                           kSettingsTableViewId);
}

// Tests for the DefaultSearchProviderEnabled policy.
// 1. Test if the policy can be properly set.
// 2. Test the managed UI item and clicking action.
- (void)testDefaultSearchProviderEnabled {
  // Disable default search provider via policy and make sure it does not crash
  // the omnibox UI.
  SetPolicy(false, policy::key::kDefaultSearchProviderEnabled);
  [ChromeEarlGrey loadURL:GURL(kChromeUIPolicyURL)];

  // Open a new tab and verify that the NTP does not crash. Regression test for
  // http://crbug.com/1148903.
  [ChromeEarlGrey openNewTab];

  // Open settings menu.
  [self openSettingsMenu];

  VerifyManagedSettingItem(kSettingsManagedSearchEngineCellId,
                           kSettingsTableViewId);
}

// Tests for the PasswordManagerEnabled policy.
- (void)testPasswordManagerEnabled {
  VerifyBoolPolicy(policy::key::kPasswordManagerEnabled,
                   password_manager::prefs::kCredentialsEnableService);
}

// Tests for the PasswordManagerEnabled policy Settings UI.
- (void)testPasswordManagerEnabledSettingsUI {
  // Force the preference off via policy.
  SetPolicy(false, policy::key::kPasswordManagerEnabled);
  GREYAssertFalse(
      [ChromeEarlGrey
          userBooleanPref:password_manager::prefs::kCredentialsEnableService],
      @"Preference was unexpectedly true");
  // Open settings menu and tap password settings.
  [self openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsMenuPasswordsButton()];

  VerifyManagedSettingItem(kSavePasswordManagedTableViewId,
                           kPasswordsTableViewId);
}

// Tests for the AutofillAddressEnabled policy Settings UI.
- (void)testAutofillAddressSettingsUI {
  // Force the preference off via policy.
  SetPolicy(false, policy::key::kAutofillAddressEnabled);
  GREYAssertFalse(
      [ChromeEarlGrey userBooleanPref:autofill::prefs::kAutofillProfileEnabled],
      @"Preference was unexpectedly true");
  // Open settings menu and tap Address and More setting.
  [self openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::AddressesAndMoreButton()];

  VerifyManagedSettingItem(kAutofillAddressManagedViewId,
                           kAutofillProfileTableViewID);
}

// Tests for the AutofillCreditCardEnabled policy Settings UI.
- (void)testAutofillCreditCardSettingsUI {
  // Force the preference off via policy.
  SetPolicy(false, policy::key::kAutofillCreditCardEnabled);
  GREYAssertFalse(
      [ChromeEarlGrey
          userBooleanPref:autofill::prefs::kAutofillCreditCardEnabled],
      @"Preference was unexpectedly true");
  // Open settings menu and tap Payment Method setting.
  [self openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::PaymentMethodsButton()];

  VerifyManagedSettingItem(kAutofillCreditCardManagedViewId,
                           kAutofillCreditCardTableViewId);
}

// Tests for the SavingBrowserHistoryDisabled policy.
- (void)testSavingBrowserHistoryDisabled {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL testURL = self.testServer->GetURL("/pony.html");
  const std::string pageText = "pony";

  // Set history to a clean state and verify it is clean.
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGrey resetBrowsingDataPrefs];
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 0,
                  @"History was unexpectedly non-empty");

  // Verify that the unmanaged pref's default value is false. While we generally
  // don't want to assert default pref values, in this case we need to start
  // from a well-known default value due to the order of the checks we make for
  // the history panel. If the default value ever changes for this pref, we'll
  // need to adjust the order of the history panel checks.
  GREYAssertFalse(
      [ChromeEarlGrey userBooleanPref:prefs::kSavingBrowserHistoryDisabled],
      @"Unexpected default value");

  // Force the preference to true via policy (disables history).
  SetPolicy(true, policy::key::kSavingBrowserHistoryDisabled);
  GREYAssertTrue(
      [ChromeEarlGrey userBooleanPref:prefs::kSavingBrowserHistoryDisabled],
      @"Disabling browser history preference was unexpectedly false");

  // Perform a navigation and make sure the history isn't changed.
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:pageText];
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 0,
                  @"History was unexpectedly non-empty");

  // Force the preference to false via policy (enables history).
  SetPolicy(false, policy::key::kSavingBrowserHistoryDisabled);
  GREYAssertFalse(
      [ChromeEarlGrey userBooleanPref:prefs::kSavingBrowserHistoryDisabled],
      @"Disabling browser history preference was unexpectedly true");

  // Perform a navigation and make sure history is being saved.
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:pageText];
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 1,
                  @"History had an unexpected entry count");
}

// Tests for the SearchSuggestEnabled policy.
- (void)testSearchSuggestEnabled {
  VerifyBoolPolicy(policy::key::kSearchSuggestEnabled,
                   prefs::kSearchSuggestEnabled);
}

// Tests that language detection is not performed and the tool manual trigger
// button is disabled when the pref kOfferTranslateEnabled is set to false.
- (void)testTranslateEnabled {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL testURL = self.testServer->GetURL("/pony.html");
  const std::string pageText = "pony";

  // Set up a fake language detection observer.
  [TranslateAppInterface
      setUpWithScriptServer:base::SysUTF8ToNSString(testURL.spec())];

  // Disable TranslateEnabled policy.
  SetPolicy(false, policy::key::kTranslateEnabled);

  // Open some webpage.
  [ChromeEarlGrey loadURL:testURL];

  // Check that no language has been detected.
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for language detection"
                  block:^BOOL() {
                    return [TranslateAppInterface isLanguageDetected];
                  }];

  GREYAssertFalse([condition waitWithTimeout:2],
                  @"The Language is unexpectedly detected.");

  // Make sure the Translate manual trigger button disabled.
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey selectElementWithMatcher:ToolsMenuTranslateButton()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  /*amount=*/200)
      onElementWithMatcher:chrome_test_util::ToolsMenuView()]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];

  // Close the tools menu.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Remove any tranlation related setup properly.
  [TranslateAppInterface tearDown];

  // Enable the policy.
  SetPolicy(true, policy::key::kTranslateEnabled);
}

- (void)testBlockPopupsSettingsUI {
  // Set the policy to int value 2, which stands for "do not allow any site to
  // show popups".
  SetPolicy(2, policy::key::kDefaultPopupsSetting);

  // Open settings menu and tap Content Settings setting.
  [self openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::ContentSettingsButton()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsBlockPopupsCellId)]
      performAction:grey_tap()];

  VerifyManagedSettingItem(@"blockPopupsContentView_managed",
                           @"block_popups_settings_view_controller");
}

// Tests that the feed is disappearing when the policy is set to false while it
// is visible.
- (void)testDisableContentSuggestions {
  // Relaunch the app with Discover enabled, as it is required for this test.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled.push_back(kDiscoverFeedInNtp);
  config.features_enabled.push_back(kRefactoredNTP);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  NSString* feedTitle = l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE);
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(feedTitle),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(kNTPCollectionViewIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  SetPolicy(false, policy::key::kNTPContentSuggestionsEnabled);

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(feedTitle),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];

  // Open settings menu and check that it is disabled.
  [self openSettingsMenu];
  VerifyManagedSettingItem(kSettingsArticleSuggestionsCellId,
                           kSettingsTableViewId);
}

- (void)testTranslateEnabledSettingsUI {
  // Disable TranslateEnabled policy.
  SetPolicy(false, policy::key::kTranslateEnabled);

  // Open settings menu and tap Languages setting.
  [self openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:chrome_test_util::LanguagesButton()];

  VerifyManagedSettingItem(kTranslateManagedAccessibilityIdentifier,
                           kLanguageSettingsTableViewAccessibilityIdentifier);
}

// Test whether the managed item will be shown if a policy is set.
- (void)testPopupMenuItem {
  // Setup a machine level policy.
  SetPolicy(false, policy::key::kTranslateEnabled);

  // Open the menu and click on the item.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:grey_accessibilityID(kTextMenuEnterpriseInfo)];
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Check the navigation.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          kChromeUIManagementURL)]
      assertWithMatcher:grey_notNil()];
}

// Test the chrome://management page when no machine level policy is set.
- (void)testManagementPageUnmanaged {
  // Open the management page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_IOS_MANAGEMENT_UI_UNMANAGED_DESC)];
}

// Test the chrome://management page when one or more machine level policies are
// set.
- (void)testManagementPageManaged {
  // Setup a machine level policy.
  SetPolicy(false, policy::key::kTranslateEnabled);

  // Open the management page and check if the content is expected.
  [ChromeEarlGrey loadURL:GURL(kChromeUIManagementURL)];
  [ChromeEarlGrey
      waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                        IDS_IOS_MANAGEMENT_UI_MESSAGE)];

  // Open the "learn more" link.
  [ChromeEarlGrey tapWebStateElementWithID:@"learn-more-link"];
}

// Tests that when the BrowserSignin policy is updated while the app is not
// launched, a policy screen is displayed at startup.
- (void)testBrowserSignInDisabledAtStartup {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Create the config to relaunch Chrome.
  AppLaunchConfiguration config;
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  // Configure the policy to disable SignIn.
  std::string policy_data = "<dict>"
                            "    <key>BrowserSignin</key>"
                            "    <integer>0</integer>"
                            "</dict>";
  base::RemoveChars(policy_data, base::kWhitespaceASCII, &policy_data);

  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(policy_data);

  // Add the switch to make sure that fakeIdentity1 is known at startup to avoid
  // automatic sign out.
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kSignInAtStartup);

  // Relaunch the app to take the configuration into account.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Check that the sign out pop up is presented.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                     IDS_IOS_ENTERPRISE_SIGNED_OUT))]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  bool promptPresented = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, condition);
  GREYAssertTrue(promptPresented, @"'Signed Out' prompt not shown");
}

// Tests that the UI notifying the user of their sign out is displayed when the
// policy changes while the app is launched.
- (void)testBrowserSignInDisabledWhileAppVisible {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Force sign out.
  SetPolicy(0, policy::key::kBrowserSignin);

  // Check that the sign out pop up is presented.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                     IDS_IOS_ENTERPRISE_SIGNED_OUT))]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  bool promptPresented = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, condition);
  GREYAssertTrue(promptPresented, @"'Signed Out' prompt not shown");
}

@end
