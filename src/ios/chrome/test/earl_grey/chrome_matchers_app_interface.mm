// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/form_suggestion_constants.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_constants.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_address_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_card_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/history/history_ui_constants.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_steady_view.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_views_utils.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/import_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/app/window_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ios/ui_image_test_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Identifer for cell at given |index| in the tab grid.
NSString* IdentifierForCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u", kGridCellIdentifierPrefix, index];
}

id<GREYMatcher> TableViewSwitchIsToggledOn(BOOL is_toggled_on) {
  GREYMatchesBlock matches = ^BOOL(id element) {
    TableViewSwitchCell* switch_cell =
        base::mac::ObjCCastStrict<TableViewSwitchCell>(element);
    UISwitch* switch_view = switch_cell.switchView;
    return (switch_view.on && is_toggled_on) ||
           (!switch_view.on && !is_toggled_on);
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    NSString* name =
        [NSString stringWithFormat:@"tableViewSwitchToggledState(%@)",
                                   is_toggled_on ? @"ON" : @"OFF"];
    [description appendText:name];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

id<GREYMatcher> TableViewSwitchIsEnabled(BOOL is_enabled) {
  GREYMatchesBlock matches = ^BOOL(id element) {
    TableViewSwitchCell* switch_cell =
        base::mac::ObjCCastStrict<TableViewSwitchCell>(element);
    UISwitch* switch_view = switch_cell.switchView;
    return (switch_view.enabled && is_enabled) ||
           (!switch_view.enabled && !is_enabled);
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    NSString* name =
        [NSString stringWithFormat:@"tableViewSwitchEnabledState(%@)",
                                   is_enabled ? @"YES" : @"NO"];
    [description appendText:name];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

// Returns the subview of |parentView| corresponding to the
// ContentSuggestionsViewController. Returns nil if it is not in its subviews.
UIView* SubviewWithAccessibilityIdentifier(NSString* accessibility_id,
                                           UIView* parent_view) {
  if (parent_view.accessibilityIdentifier == accessibility_id) {
    return parent_view;
  }
  for (UIView* view in parent_view.subviews) {
    UIView* result_view =
        SubviewWithAccessibilityIdentifier(accessibility_id, view);
    if (result_view)
      return result_view;
  }
  return nil;
}

UIWindow* WindowWithAccessibilityIdentifier(NSString* accessibility_id) {
  for (UIWindow* window in UIApplication.sharedApplication.windows) {
    if ([window.accessibilityIdentifier isEqualToString:accessibility_id])
      return window;
  }
  return nil;
}

}  // namespace

@implementation ChromeMatchersAppInterface

+ (id<GREYMatcher>)windowWithNumber:(int)windowNumber {
  return grey_allOf(
      grey_accessibilityID([NSString stringWithFormat:@"%d", windowNumber]),
      grey_kindOfClass([UIWindow class]), nil);
}

+ (id<GREYMatcher>)blockerWindowWithNumber:(int)windowNumber {
  return grey_allOf(grey_accessibilityID([NSString
                        stringWithFormat:@"blocker-%d", windowNumber]),
                    grey_kindOfClass([UIWindow class]), nil);
}

+ (id<GREYMatcher>)buttonWithAccessibilityLabel:(NSString*)label {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

+ (id<GREYMatcher>)buttonWithAccessibilityLabelID:(int)messageID {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabel:l10n_util::GetNSStringWithFixup(messageID)];
}

+ (id<GREYMatcher>)imageViewWithImageNamed:(NSString*)imageName {
  UIImage* expectedImage = [UIImage imageNamed:imageName];
  GREYMatchesBlock matches = ^BOOL(UIImageView* imageView) {
    return ui::test::uiimage_utils::UIImagesAreEqual(expectedImage,
                                                     imageView.image);
  };
  NSString* descriptionString =
      [NSString stringWithFormat:@"Images matching image named %@", imageName];
  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:descriptionString];
  };
  id<GREYMatcher> imageMatcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return imageMatcher;
}

+ (id<GREYMatcher>)imageViewWithImage:(int)imageID {
  UIImage* expectedImage = NativeImage(imageID);
  GREYMatchesBlock matches = ^BOOL(UIImageView* imageView) {
    return ui::test::uiimage_utils::UIImagesAreEqual(expectedImage,
                                                     imageView.image);
  };
  NSString* descriptionString =
      [NSString stringWithFormat:@"Images matching %i", imageID];
  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:descriptionString];
  };
  id<GREYMatcher> imageMatcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return imageMatcher;
}

+ (id<GREYMatcher>)buttonWithImage:(int)imageID {
  UIImage* expectedImage = NativeImage(imageID);
  GREYMatchesBlock matches = ^BOOL(UIButton* button) {
    return ui::test::uiimage_utils::UIImagesAreEqual(expectedImage,
                                                     [button currentImage]);
  };
  NSString* descriptionString =
      [NSString stringWithFormat:@"Images matching %i", imageID];
  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:descriptionString];
  };
  id<GREYMatcher> imageMatcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return grey_allOf(grey_accessibilityTrait(UIAccessibilityTraitButton),
                    imageMatcher, nil);
}

+ (id<GREYMatcher>)staticTextWithAccessibilityLabelID:(int)messageID {
  return [ChromeMatchersAppInterface
      staticTextWithAccessibilityLabel:(l10n_util::GetNSStringWithFixup(
                                           messageID))];
}

+ (id<GREYMatcher>)staticTextWithAccessibilityLabel:(NSString*)label {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitStaticText),
                    nil);
}

+ (id<GREYMatcher>)headerWithAccessibilityLabelID:(int)messageID {
  return [ChromeMatchersAppInterface
      headerWithAccessibilityLabel:(l10n_util::GetNSStringWithFixup(
                                       messageID))];
}

+ (id<GREYMatcher>)headerWithAccessibilityLabel:(NSString*)label {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitHeader), nil);
}

+ (id<GREYMatcher>)navigationBarTitleWithAccessibilityLabelID:(int)labelID {
  return grey_allOf(
      grey_accessibilityID(l10n_util::GetNSStringWithFixup(labelID)),
      grey_kindOfClass([UINavigationBar class]), nil);
}

+ (id<GREYMatcher>)textFieldForCellWithLabelID:(int)messageID {
  return grey_allOf(grey_accessibilityID([l10n_util::GetNSStringWithFixup(
                        messageID) stringByAppendingString:@"_textField"]),
                    grey_kindOfClass([UITextField class]), nil);
}

+ (id<GREYMatcher>)iconViewForCellWithLabelID:(int)messageID
                                     iconType:(NSString*)iconType {
  return grey_allOf(grey_accessibilityID([l10n_util::GetNSStringWithFixup(
                        messageID) stringByAppendingString:iconType]),
                    grey_kindOfClass([UIImageView class]), nil);
}

+ (id<GREYMatcher>)primaryToolbar {
  return grey_kindOfClass([PrimaryToolbarView class]);
}

+ (id<GREYMatcher>)cancelButton {
  return
      [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:(IDS_CANCEL)];
}

+ (id<GREYMatcher>)navigationBarCancelButton {
  return grey_allOf(
      grey_ancestor(grey_kindOfClass([UINavigationBar class])),
      [self cancelButton],
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

+ (id<GREYMatcher>)closeButton {
  return grey_allOf(
      [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:(IDS_CLOSE)],
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

+ (id<GREYMatcher>)closeTabMenuButton {
  return grey_allOf(
      [ChromeMatchersAppInterface
          buttonWithAccessibilityLabelID:(IDS_IOS_CONTENT_CONTEXT_CLOSETAB)],
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)forwardButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_ACCNAME_FORWARD)];
}

+ (id<GREYMatcher>)backButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_ACCNAME_BACK)];
}

+ (id<GREYMatcher>)reloadButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_ACCNAME_RELOAD)];
}

+ (id<GREYMatcher>)stopButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_ACCNAME_STOP)];
}

+ (id<GREYMatcher>)omnibox {
  return grey_allOf(grey_kindOfClass([OmniboxTextFieldIOS class]),
                    grey_userInteractionEnabled(), nil);
}

+ (id<GREYMatcher>)defocusedLocationView {
  return grey_kindOfClass([LocationBarSteadyView class]);
}

+ (id<GREYMatcher>)pageSecurityInfoButton {
  return grey_accessibilityLabel(@"Page Security Info");
}

+ (id<GREYMatcher>)pageSecurityInfoIndicator {
  return grey_accessibilityLabel(@"Page Security Info");
}

+ (id<GREYMatcher>)omniboxText:(NSString*)text {
  GREYElementMatcherBlock* matcher = [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(id element) {
        OmniboxTextFieldIOS* omnibox =
            base::mac::ObjCCast<OmniboxTextFieldIOS>(element);
        return [omnibox.text isEqualToString:text];
      }
      descriptionBlock:^void(id<GREYDescription> description) {
        [description
            appendText:[NSString stringWithFormat:@"Omnibox contains text '%@'",
                                                  text]];
      }];
  return matcher;
}

+ (id<GREYMatcher>)omniboxContainingText:(NSString*)text {
  GREYElementMatcherBlock* matcher = [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(id element) {
        OmniboxTextFieldIOS* omnibox =
            base::mac::ObjCCast<OmniboxTextFieldIOS>(element);
        return [omnibox.text containsString:text];
      }
      descriptionBlock:^void(id<GREYDescription> description) {
        [description
            appendText:[NSString stringWithFormat:@"Omnibox contains text '%@'",
                                                  text]];
      }];
  return matcher;
}

+ (id<GREYMatcher>)locationViewContainingText:(NSString*)text {
  GREYElementMatcherBlock* matcher = [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(LocationBarSteadyView* element) {
        return [element.locationLabel.text containsString:text];
      }
      descriptionBlock:^void(id<GREYDescription> description) {
        [description
            appendText:[NSString
                           stringWithFormat:
                               @"LocationBarSteadyView contains text '%@'",
                               text]];
      }];
  return matcher;
}

+ (id<GREYMatcher>)toolsMenuButton {
  return grey_allOf(grey_accessibilityID(kToolbarToolsMenuButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)openNewTabButton {
  return grey_allOf(
      [ChromeMatchersAppInterface
          buttonWithAccessibilityLabelID:(IDS_IOS_TOOLS_MENU_NEW_TAB)],
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)shareButton {
  return grey_allOf(
      [ChromeMatchersAppInterface
          buttonWithAccessibilityLabelID:(IDS_IOS_TOOLS_MENU_SHARE)],
      grey_not([self tabShareButton]), grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabShareButton {
  return grey_allOf(
      grey_anyOf(grey_accessibilityID(kToolbarShareButtonIdentifier),
                 grey_accessibilityID(kOmniboxShareButtonIdentifier), nil),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)showTabsButton {
  return grey_allOf(grey_accessibilityID(kToolbarStackButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)addToReadingListButton {
  return grey_allOf([ChromeMatchersAppInterface
                        buttonWithAccessibilityLabelID:
                            (IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST)],
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)addToBookmarksButton {
  return grey_allOf(
      [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:
                                      (IDS_IOS_CONTENT_CONTEXT_ADDTOBOOKMARKS)],
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tableViewSwitchCell:(NSString*)accessibilityIdentifier
                           isToggledOn:(BOOL)isToggledOn {
  return [ChromeMatchersAppInterface tableViewSwitchCell:accessibilityIdentifier
                                             isToggledOn:isToggledOn
                                               isEnabled:YES];
}

+ (id<GREYMatcher>)tableViewSwitchCell:(NSString*)accessibilityIdentifier
                           isToggledOn:(BOOL)isToggledOn
                             isEnabled:(BOOL)isEnabled {
  return grey_allOf(grey_accessibilityID(accessibilityIdentifier),
                    TableViewSwitchIsToggledOn(isToggledOn),
                    TableViewSwitchIsEnabled(isEnabled),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)syncSwitchCell:(NSString*)accessibilityLabel
                      isToggledOn:(BOOL)isToggledOn {
  return grey_allOf(
      grey_accessibilityLabel(accessibilityLabel),
      grey_accessibilityValue(
          isToggledOn ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                      : l10n_util::GetNSString(IDS_IOS_SETTING_OFF)),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)openLinkInNewTabButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)];
}

+ (id<GREYMatcher>)openLinkInIncognitoButton {
  int stringId = IDS_IOS_OPEN_IN_INCOGNITO_ACTION_TITLE;
  return [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:(stringId)];
}

+ (id<GREYMatcher>)openLinkInNewWindowButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_CONTENT_CONTEXT_OPENINNEWWINDOW)];
}

+ (id<GREYMatcher>)navigationBarDoneButton {
  return grey_allOf(
      [ChromeMatchersAppInterface
          buttonWithAccessibilityLabelID:(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)],
      grey_userInteractionEnabled(), grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)bookmarksNavigationBarDoneButton {
  return grey_accessibilityID(kBookmarkHomeNavigationBarDoneButtonIdentifier);
}

+ (id<GREYMatcher>)bookmarksNavigationBarBackButton {
  UINavigationBar* navBar = base::mac::ObjCCastStrict<UINavigationBar>(
      SubviewWithAccessibilityIdentifier(kBookmarkNavigationBarIdentifier,
                                         GetAnyKeyWindow()));
  return grey_allOf(grey_buttonTitle(navBar.backItem.title),
                    grey_ancestor(grey_kindOfClass([UINavigationBar class])),
                    nil);
}

+ (id<GREYMatcher>)addAccountButton {
  return grey_accessibilityID(kSettingsAccountsTableViewAddAccountCellId);
}

+ (id<GREYMatcher>)signOutAccountsButton {
  return grey_accessibilityID(kSettingsAccountsTableViewSignoutCellId);
}

+ (id<GREYMatcher>)clearBrowsingDataCell {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_CLEAR_BROWSING_DATA_TITLE)];
}

+ (id<GREYMatcher>)clearBrowsingDataButton {
  return grey_accessibilityID(kClearBrowsingDataButtonIdentifier);
}

+ (id<GREYMatcher>)clearBrowsingDataView {
  return grey_accessibilityID(kClearBrowsingDataViewAccessibilityIdentifier);
}

+ (id<GREYMatcher>)confirmClearBrowsingDataButton {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON)),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_not(grey_accessibilityID(kClearBrowsingDataButtonIdentifier)),
      grey_userInteractionEnabled(), nil);
}

+ (id<GREYMatcher>)settingsMenuButton {
  return grey_accessibilityID(kToolsMenuSettingsId);
}

+ (id<GREYMatcher>)settingsDoneButton {
  return grey_accessibilityID(kSettingsDoneButtonId);
}

+ (id<GREYMatcher>)autofillCreditCardEditTableView {
  return grey_accessibilityID(kAutofillCreditCardEditTableViewId);
}

+ (id<GREYMatcher>)addressesAndMoreButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE)];
}

+ (id<GREYMatcher>)paymentMethodsButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_AUTOFILL_PAYMENT_METHODS)];
}

+ (id<GREYMatcher>)languagesButton {
  return grey_accessibilityID(kSettingsLanguagesCellId);
}

+ (id<GREYMatcher>)addCreditCardView {
  return grey_accessibilityID(kAddCreditCardViewID);
}

+ (id<GREYMatcher>)autofillCreditCardTableView {
  return grey_accessibilityID(kAutofillCreditCardTableViewId);
}

+ (id<GREYMatcher>)addPaymentMethodButton {
  return grey_accessibilityID(kSettingsAddPaymentMethodButtonId);
}

+ (id<GREYMatcher>)addCreditCardButton {
  return grey_accessibilityID(kSettingsAddCreditCardButtonID);
}

+ (id<GREYMatcher>)addCreditCardCancelButton {
  return grey_accessibilityID(kSettingsAddCreditCardCancelButtonID);
}

+ (id<GREYMatcher>)toolsMenuView {
  return grey_accessibilityID(kPopupMenuToolsMenuTableViewId);
}

+ (id<GREYMatcher>)OKButton {
  return [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:(IDS_OK)];
}

+ (id<GREYMatcher>)primarySignInButton {
  return grey_allOf(grey_accessibilityID(kSigninPromoPrimaryButtonId),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)secondarySignInButton {
  return grey_allOf(grey_accessibilityID(kSigninPromoSecondaryButtonId),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)settingsAccountButton {
  return grey_accessibilityID(kSettingsAccountCellId);
}

+ (id<GREYMatcher>)settingsAccountsCollectionView {
  return grey_accessibilityID(kSettingsAccountsTableViewId);
}

+ (id<GREYMatcher>)settingsImportDataImportButton {
  return grey_accessibilityID(kImportDataImportCellId);
}

+ (id<GREYMatcher>)settingsImportDataKeepSeparateButton {
  return grey_accessibilityID(kImportDataKeepSeparateCellId);
}

+ (id<GREYMatcher>)settingsImportDataContinueButton {
  return grey_accessibilityID(kImportDataContinueButtonId);
}

+ (id<GREYMatcher>)settingsPrivacyTableView {
  return grey_accessibilityID(kPrivacyTableViewId);
}

+ (id<GREYMatcher>)contentSettingsButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_CONTENT_SETTINGS_TITLE)];
}

+ (id<GREYMatcher>)googleServicesSettingsButton {
  return grey_allOf(grey_kindOfClass([UITableViewCell class]),
                    grey_accessibilityID(kSettingsGoogleServicesCellId),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)manageSyncSettingsButton {
  return grey_allOf(grey_kindOfClass([UITableViewCell class]),
                    grey_accessibilityID(kSettingsGoogleSyncAndServicesCellId),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)googleServicesSettingsView {
  return grey_accessibilityID(kGoogleServicesSettingsViewIdentifier);
}

+ (id<GREYMatcher>)settingsMenuBackButton:(NSString*)buttonTitle {
  return grey_allOf(
      grey_anyOf(grey_accessibilityLabel(buttonTitle),
                 grey_accessibilityLabel(@"Back"), grey_buttonTitle(@"Back"),
                 grey_descendant(grey_buttonTitle(buttonTitle)), nil),
      grey_kindOfClassName(@"_UIButtonBarButton"),
      grey_ancestor(grey_kindOfClass([UINavigationBar class])), nil);
}

+ (id<GREYMatcher>)settingsMenuBackButton {
  UINavigationBar* navBar = base::mac::ObjCCastStrict<UINavigationBar>(
      SubviewWithAccessibilityIdentifier(@"SettingNavigationBar",
                                         GetAnyKeyWindow()));
  return
      [ChromeMatchersAppInterface settingsMenuBackButton:navBar.backItem.title];
}

+ (id<GREYMatcher>)settingsMenuBackButtonInWindowWithNumber:(int)windowNumber {
  UINavigationBar* navBar = base::mac::ObjCCastStrict<UINavigationBar>(
      SubviewWithAccessibilityIdentifier(
          @"SettingNavigationBar", WindowWithAccessibilityIdentifier([NSString
                                       stringWithFormat:@"%d", windowNumber])));
  return
      [ChromeMatchersAppInterface settingsMenuBackButton:navBar.backItem.title];
}

+ (id<GREYMatcher>)settingsMenuPrivacyButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:
          (IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY)];
}

+ (id<GREYMatcher>)settingsMenuPasswordsButton {
  return grey_accessibilityID(kSettingsPasswordsCellId);
}

// TODO(crbug.com/1021752): Remove this stub.
+ (id<GREYMatcher>)paymentRequestView {
  return nil;
}

// TODO(crbug.com/1021752): Remove this stub.
+ (id<GREYMatcher>)paymentRequestErrorView {
  return nil;
}

+ (id<GREYMatcher>)voiceSearchButton {
  return grey_allOf(grey_accessibilityID(kSettingsVoiceSearchCellId),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

+ (id<GREYMatcher>)voiceSearchInputAccessoryButton {
  return grey_accessibilityID(kVoiceSearchInputAccessoryViewID);
}

+ (id<GREYMatcher>)settingsCollectionView {
  return grey_allOf(grey_accessibilityID(kSettingsTableViewId),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)clearBrowsingHistoryButton {
  // Needs to use grey_sufficientlyVisible() to make the difference between a
  // cell used by the tableview and a invisible recycled cell.
  return grey_allOf(
      grey_accessibilityID(kClearBrowsingHistoryCellAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)clearCookiesButton {
  // Needs to use grey_sufficientlyVisible() to make the difference between a
  // cell used by the tableview and a invisible recycled cell.
  return grey_allOf(
      grey_accessibilityID(kClearCookiesCellAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)clearCacheButton {
  // Needs to use grey_sufficientlyVisible() to make the difference between a
  // cell used by the tableview and a invisible recycled cell.
  return grey_allOf(
      grey_accessibilityID(kClearCacheCellAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)clearSavedPasswordsButton {
  // Needs to use grey_sufficientlyVisible() to make the difference between a
  // cell used by the tableview and a invisible recycled cell.
  return grey_allOf(
      grey_accessibilityID(kClearSavedPasswordsCellAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)clearAutofillButton {
  // Needs to use grey_sufficientlyVisible() to make the difference between a
  // cell used by the tableview and a invisible recycled cell.
  return grey_allOf(
      grey_accessibilityID(kClearAutofillCellAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)contentSuggestionCollectionView {
  return grey_accessibilityID(kContentSuggestionsCollectionIdentifier);
}

+ (id<GREYMatcher>)ntpCollectionView {
  return grey_accessibilityID(kNTPCollectionViewIdentifier);
}

+ (id<GREYMatcher>)ntpIncognitoView {
  return grey_accessibilityID(kNTPIncognitoViewIdentifier);
}

+ (id<GREYMatcher>)ntpFeedMenuEnableButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:IDS_IOS_DISCOVER_FEED_MENU_TURN_ON_ITEM];
}

+ (id<GREYMatcher>)ntpFeedMenuDisableButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:IDS_IOS_DISCOVER_FEED_MENU_TURN_OFF_ITEM];
}

// TODO(crbug.com/1021752): Remove this stub.
+ (id<GREYMatcher>)warningMessageView {
  return nil;
}

// TODO(crbug.com/1021752): Remove this stub.
+ (id<GREYMatcher>)paymentRequestPickerRow {
  return nil;
}

// TODO(crbug.com/1021752): Remove this stub.
+ (id<GREYMatcher>)paymentRequestPickerSearchBar {
  return nil;
}

+ (id<GREYMatcher>)openNewWindowMenuButton {
  return grey_accessibilityID(kToolsMenuNewWindowId);
}

+ (id<GREYMatcher>)readingListMenuButton {
  return grey_accessibilityID(kToolsMenuReadingListId);
}

+ (id<GREYMatcher>)bookmarksMenuButton {
  return grey_accessibilityID(kToolsMenuBookmarksId);
}

+ (id<GREYMatcher>)recentTabsMenuButton {
  return grey_accessibilityID(kToolsMenuOtherDevicesId);
}

+ (id<GREYMatcher>)systemSelectionCallout {
  return grey_kindOfClass(NSClassFromString(@"UICalloutBarButton"));
}

+ (id<GREYMatcher>)systemSelectionCalloutLinkToTextButton {
  return grey_allOf(grey_accessibilityLabel(
                        l10n_util::GetNSString(IDS_IOS_SHARE_LINK_TO_TEXT)),
                    [self systemSelectionCallout], nil);
}

+ (id<GREYMatcher>)systemSelectionCalloutCopyButton {
  return grey_allOf(grey_accessibilityLabel(@"Copy"),
                    [self systemSelectionCallout], nil);
}

+ (id<GREYMatcher>)systemSelectionCalloutOverflowButton {
  return grey_accessibilityID(@"show.next.items.menu.button");
}

+ (id<GREYMatcher>)copyActivityButton {
  id<GREYMatcher> copyStaticText = [ChromeMatchersAppInterface
      staticTextWithAccessibilityLabel:l10n_util::GetNSString(
                                           IDS_IOS_SHARE_MENU_COPY)];
  return grey_allOf(
      copyStaticText,
      grey_ancestor(grey_kindOfClassName(@"UIActivityActionGroupCell")), nil);
}

+ (id<GREYMatcher>)copyLinkButton {
  int stringId = IDS_IOS_COPY_LINK_ACTION_TITLE;
  return [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:stringId];
}

+ (id<GREYMatcher>)editButton {
  int stringId = IDS_IOS_EDIT_ACTION_TITLE;
  return [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:stringId];
}

+ (id<GREYMatcher>)moveButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE];
}

+ (id<GREYMatcher>)readingListMarkAsReadButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:IDS_IOS_READING_LIST_MARK_AS_READ_ACTION];
}

+ (id<GREYMatcher>)readingListMarkAsUnreadButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:
          IDS_IOS_READING_LIST_MARK_AS_UNREAD_ACTION];
}

+ (id<GREYMatcher>)deleteButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:IDS_IOS_DELETE_ACTION_TITLE];
}

+ (id<GREYMatcher>)contextMenuCopyButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_CONTENT_CONTEXT_COPY)];
}

+ (id<GREYMatcher>)NTPOmnibox {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT)),
      grey_minimumVisiblePercent(0.2), nil);
}

+ (id<GREYMatcher>)fakeOmnibox {
  return grey_accessibilityID(ntp_home::FakeOmniboxAccessibilityID());
}

+ (id<GREYMatcher>)discoverHeaderLabel {
  return grey_accessibilityID(ntp_home::DiscoverHeaderTitleAccessibilityID());
}

+ (id<GREYMatcher>)ntpLogo {
  return grey_accessibilityID(ntp_home::NTPLogoAccessibilityID());
}

+ (id<GREYMatcher>)webViewMatcher {
  return web::WebViewInWebState(chrome_test_util::GetCurrentWebState());
}

+ (id<GREYMatcher>)webStateScrollViewMatcher {
  return web::WebViewScrollView(chrome_test_util::GetCurrentWebState());
}

+ (id<GREYMatcher>)webStateScrollViewMatcherInWindowWithNumber:
    (int)windowNumber {
  return web::WebViewScrollView(
      chrome_test_util::GetCurrentWebStateForWindowWithNumber(windowNumber));
}

+ (id<GREYMatcher>)historyClearBrowsingDataButton {
  return grey_accessibilityID(kHistoryToolbarClearBrowsingButtonIdentifier);
}

+ (id<GREYMatcher>)openInButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:IDS_IOS_OPEN_IN];
}

+ (id<GREYMatcher>)tabGridCellAtIndex:(unsigned int)index {
  return grey_allOf(grey_accessibilityID(IdentifierForCellAtIndex(index)),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridDoneButton {
  return grey_allOf(grey_accessibilityID(kTabGridDoneButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridCloseAllButton {
  return grey_allOf(grey_accessibilityID(kTabGridCloseAllButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridUndoCloseAllButton {
  return grey_allOf(grey_accessibilityID(kTabGridUndoCloseAllButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridSelectShowHistoryCell {
  return grey_allOf(grey_accessibilityID(
                        kRecentTabsShowFullHistoryCellAccessibilityIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridRegularTabsEmptyStateView {
  return grey_allOf(
      grey_accessibilityID(kTabGridRegularTabsEmptyStateIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridNewTabButton {
  return grey_allOf(
      [self buttonWithAccessibilityLabelID:IDS_IOS_TAB_GRID_CREATE_NEW_TAB],
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridNewIncognitoTabButton {
  return grey_allOf([self buttonWithAccessibilityLabelID:
                              IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB],
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridOpenTabsPanelButton {
  return grey_accessibilityID(kTabGridRegularTabsPageButtonIdentifier);
}

+ (id<GREYMatcher>)tabGridIncognitoTabsPanelButton {
  return grey_accessibilityID(kTabGridIncognitoTabsPageButtonIdentifier);
}

+ (id<GREYMatcher>)tabGridOtherDevicesPanelButton {
  return grey_accessibilityID(kTabGridRemoteTabsPageButtonIdentifier);
}

+ (id<GREYMatcher>)tabGridBackground {
  return grey_accessibilityID(kGridBackgroundIdentifier);
}

+ (id<GREYMatcher>)regularTabGrid {
  return grey_accessibilityID(kRegularTabGridIdentifier);
}

+ (id<GREYMatcher>)incognitoTabGrid {
  return grey_accessibilityID(kIncognitoTabGridIdentifier);
}

+ (id<GREYMatcher>)tabGridCloseButtonForCellAtIndex:(unsigned int)index {
  return grey_allOf(
      grey_ancestor(grey_accessibilityID(IdentifierForCellAtIndex(index))),
      grey_accessibilityID(kGridCellCloseButtonIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)settingsPasswordMatcher {
  return grey_accessibilityID(kPasswordsTableViewId);
}

+ (id<GREYMatcher>)settingsPasswordSearchMatcher {
  return grey_accessibilityID(kPasswordsSearchBarId);
}

+ (id<GREYMatcher>)settingsProfileMatcher {
  return grey_accessibilityID(kAutofillProfileTableViewID);
}

+ (id<GREYMatcher>)settingsCreditCardMatcher {
  return grey_accessibilityID(kAutofillCreditCardTableViewId);
}

+ (id<GREYMatcher>)autofillSuggestionViewMatcher {
  return grey_accessibilityID(kFormSuggestionLabelAccessibilityIdentifier);
}

+ (id<GREYMatcher>)settingsBottomToolbarDeleteButton {
  return grey_accessibilityID(kSettingsToolbarDeleteButtonId);
}

+ (id<GREYMatcher>)settingsSearchEngineButton {
  return grey_accessibilityID(kSettingsSearchEngineCellId);
}

+ (id<GREYMatcher>)contentViewSmallerThanScrollView {
  GREYMatchesBlock matches = ^BOOL(UIView* view) {
    UIScrollView* scrollView = base::mac::ObjCCast<UIScrollView>(view);
    return scrollView &&
           scrollView.contentSize.height < scrollView.bounds.size.height;
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description
        appendText:@"Not a scroll view or the scroll view content is bigger "
                   @"than the scroll view bounds"];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

+ (id<GREYMatcher>)historyEntryForURL:(NSString*)URL title:(NSString*)title {
  GREYMatchesBlock matches = ^BOOL(TableViewURLCell* cell) {
    return [cell.titleLabel.text isEqual:title] &&
           [cell.URLLabel.text isEqual:URL];
  };

  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"view containing URL text: "];
    [description appendText:URL];
    [description appendText:@" title text: "];
    [description appendText:title];
  };
  return grey_allOf(
      grey_kindOfClass([TableViewURLCell class]),
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe],
      grey_sufficientlyVisible(), nil);
}

#pragma mark - Manual Fallback

+ (id<GREYMatcher>)manualFallbackFormSuggestionViewMatcher {
  return grey_accessibilityID(kFormSuggestionsViewAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackPasswordIconMatcher {
  return grey_accessibilityID(
      manual_fill::AccessoryPasswordAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackKeyboardIconMatcher {
  return grey_accessibilityID(
      manual_fill::AccessoryKeyboardAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackPasswordTableViewMatcher {
  return grey_accessibilityID(
      manual_fill::kPasswordTableViewAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackPasswordSearchBarMatcher {
  return grey_accessibilityID(
      manual_fill::kPasswordSearchBarAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackManagePasswordsMatcher {
  return grey_accessibilityID(
      manual_fill::ManagePasswordsAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackOtherPasswordsMatcher {
  return grey_accessibilityID(
      manual_fill::OtherPasswordsAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackOtherPasswordsDismissMatcher {
  return grey_accessibilityID(
      manual_fill::kPasswordDoneButtonAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackPasswordButtonMatcher {
  return grey_buttonTitle(kMaskedPasswordTitle);
}

+ (id<GREYMatcher>)manualFallbackPasswordTableViewWindowMatcher {
  id<GREYMatcher> classMatcher = grey_kindOfClass([UIWindow class]);
  id<GREYMatcher> parentMatcher =
      grey_descendant([self manualFallbackPasswordTableViewMatcher]);
  return grey_allOf(classMatcher, parentMatcher, nil);
}

+ (id<GREYMatcher>)manualFallbackProfilesIconMatcher {
  return grey_accessibilityID(
      manual_fill::AccessoryAddressAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackProfilesTableViewMatcher {
  return grey_accessibilityID(
      manual_fill::AddressTableViewAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackManageProfilesMatcher {
  return grey_accessibilityID(
      manual_fill::ManageAddressAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackProfileTableViewWindowMatcher {
  id<GREYMatcher> classMatcher = grey_kindOfClass([UIWindow class]);
  id<GREYMatcher> parentMatcher =
      grey_descendant([self manualFallbackProfilesTableViewMatcher]);
  return grey_allOf(classMatcher, parentMatcher, nil);
}

+ (id<GREYMatcher>)manualFallbackCreditCardIconMatcher {
  return grey_accessibilityID(
      manual_fill::AccessoryCreditCardAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackCreditCardTableViewMatcher {
  return grey_accessibilityID(
      manual_fill::CardTableViewAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackManageCreditCardsMatcher {
  return grey_accessibilityID(manual_fill::ManageCardsAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackAddCreditCardsMatcher {
  return grey_accessibilityID(
      manual_fill::kAddCreditCardsAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackCreditCardTableViewWindowMatcher {
  id<GREYMatcher> classMatcher = grey_kindOfClass([UIWindow class]);
  id<GREYMatcher> parentMatcher =
      grey_descendant([self manualFallbackCreditCardTableViewMatcher]);
  return grey_allOf(classMatcher, parentMatcher, nil);
}

+ (id<GREYMatcher>)activityViewHeaderWithURLHost:(NSString*)host
                                           title:(NSString*)pageTitle {
#if TARGET_IPHONE_SIMULATOR
  return grey_allOf(
      // The title of the activity view starts as the URL, then asynchronously
      // changes to the page title. Sometimes, the activity view fails to update
      // the text to the page title, causing test flake. Allow matcher to pass
      // with either value for the activity view title.
      grey_anyOf(grey_accessibilityLabel(host),
                 grey_accessibilityLabel(pageTitle), nil),
      grey_ancestor(
          grey_allOf(grey_accessibilityTrait(UIAccessibilityTraitHeader),
                     grey_kindOfClassName(@"LPLinkView"), nil)),
      nil);
#else
  // Device tests tend to fail more often if the host is allowed in the
  // grey_anyOf as above.
  return grey_allOf(grey_accessibilityLabel(pageTitle),
                    grey_ancestor(grey_allOf(
                        grey_accessibilityTrait(UIAccessibilityTraitHeader),
                        grey_kindOfClassName(@"LPLinkView"), nil)),
                    nil);
#endif
}

+ (id<GREYMatcher>)manualFallbackSuggestPasswordMatcher {
  return grey_accessibilityID(
      manual_fill::SuggestPasswordAccessibilityIdentifier);
}

+ (id<GREYMatcher>)useSuggestedPasswordMatcher {
  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kIOSEnablePasswordManagerBrandingUpdate)) {
    return grey_allOf([self buttonWithAccessibilityLabelID:
                                IDS_IOS_USE_SUGGESTED_STRONG_PASSWORD],
                      grey_interactable(), nullptr);
  }

  return grey_allOf(
      [self buttonWithAccessibilityLabelID:IDS_IOS_USE_SUGGESTED_PASSWORD],
      grey_interactable(), nullptr);
}

#pragma mark - Tab Grid Selection Mode
+ (id<GREYMatcher>)tabGridEditButton {
  return grey_accessibilityID(kTabGridEditButtonIdentifier);
}

+ (id<GREYMatcher>)tabGridEditMenuCloseAllButton {
  return grey_allOf(
      [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:
                                      (IDS_IOS_CONTENT_CONTEXT_CLOSEALLTABS)],
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridSelectTabsMenuButton {
  return grey_allOf(
      [ChromeMatchersAppInterface
          buttonWithAccessibilityLabelID:(IDS_IOS_CONTENT_CONTEXT_SELECTTABS)],
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridEditAddToButton {
  return grey_allOf(grey_accessibilityID(kTabGridEditAddToButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridEditCloseTabsButton {
  return grey_allOf(grey_accessibilityID(kTabGridEditCloseTabsButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridEditSelectAllButton {
  return grey_allOf(grey_accessibilityID(kTabGridEditSelectAllButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridEditShareButton {
  return grey_allOf(grey_accessibilityID(kTabGridEditShareButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

@end
