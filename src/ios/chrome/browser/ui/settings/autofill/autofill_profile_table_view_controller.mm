// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_table_view_controller.h"

#include "base/check.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/cells/autofill_profile_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSwitches = kSectionIdentifierEnumZero,
  SectionIdentifierProfiles,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAutofillAddressSwitch = kItemTypeEnumZero,
  ItemTypeAutofillAddressManaged,
  ItemTypeAddress,
  ItemTypeHeader,
  ItemTypeFooter,
};

}  // namespace

#pragma mark - AutofillProfileTableViewController

@interface AutofillProfileTableViewController () <
    PersonalDataManagerObserver,
    PopoverLabelViewControllerDelegate> {
  autofill::PersonalDataManager* _personalDataManager;

  ChromeBrowserState* _browserState;
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge> _observer;

  // Deleting profiles updates PersonalDataManager resulting in an observer
  // callback, which handles general data updates with a reloadData.
  // It is better to handle user-initiated changes with more specific actions
  // such as inserting or removing items/sections. This boolean is used to
  // stop the observer callback from acting on user-initiated changes.
  BOOL _deletionInProgress;
}

@property(nonatomic, getter=isAutofillProfileEnabled)
    BOOL autofillProfileEnabled;

@end

@implementation AutofillProfileTableViewController

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  DCHECK(browserState);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE);
    self.shouldHideDoneButton = YES;
    _browserState = browserState;
    _personalDataManager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(_browserState);
    _observer.reset(new autofill::PersonalDataManagerObserverBridge(self));
    _personalDataManager->AddObserver(_observer.get());
  }
  return self;
}

- (void)dealloc {
  _personalDataManager->RemoveObserver(_observer.get());
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.accessibilityIdentifier = kAutofillProfileTableViewID;
  self.tableView.estimatedSectionFooterHeight =
      kTableViewHeaderFooterViewHeight;
  [self updateUIForEditState];
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSwitches];

  if (base::FeatureList::IsEnabled(kEnableIOSManagedSettingsUI) &&
      _browserState->GetPrefs()->IsManagedPreference(
          autofill::prefs::kAutofillProfileEnabled)) {
    [model addItem:[self managedAddressItem]
        toSectionWithIdentifier:SectionIdentifierSwitches];
  } else {
    [model addItem:[self addressSwitchItem]
        toSectionWithIdentifier:SectionIdentifierSwitches];
  }

  [model setFooter:[self addressSwitchFooter]
      forSectionWithIdentifier:SectionIdentifierSwitches];

  [self populateProfileSection];
}

#pragma mark - LoadModel Helpers

// Populates profile section using personalDataManager.
- (void)populateProfileSection {
  TableViewModel* model = self.tableViewModel;
  const std::vector<autofill::AutofillProfile*> autofillProfiles =
      _personalDataManager->GetProfiles();
  if (!autofillProfiles.empty()) {
    [model addSectionWithIdentifier:SectionIdentifierProfiles];
    [model setHeader:[self profileSectionHeader]
        forSectionWithIdentifier:SectionIdentifierProfiles];
    for (autofill::AutofillProfile* autofillProfile : autofillProfiles) {
      DCHECK(autofillProfile);
      [model addItem:[self itemForProfile:*autofillProfile]
          toSectionWithIdentifier:SectionIdentifierProfiles];
    }
  }
}

- (TableViewItem*)addressSwitchItem {
  SettingsSwitchItem* switchItem =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeAutofillAddressSwitch];
  switchItem.text =
      l10n_util::GetNSString(IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_LABEL);
  switchItem.on = [self isAutofillProfileEnabled];
  switchItem.accessibilityIdentifier = kAutofillAddressSwitchViewId;
  return switchItem;
}

- (TableViewInfoButtonItem*)managedAddressItem {
  TableViewInfoButtonItem* managedAddressItem = [[TableViewInfoButtonItem alloc]
      initWithType:ItemTypeAutofillAddressManaged];
  managedAddressItem.text =
      l10n_util::GetNSString(IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_LABEL);
  // The status could only be off when the pref is managed.
  managedAddressItem.statusText = l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  managedAddressItem.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
  managedAddressItem.accessibilityIdentifier = kAutofillAddressManagedViewId;
  return managedAddressItem;
}

- (TableViewHeaderFooterItem*)addressSwitchFooter {
  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footer.text =
      l10n_util::GetNSString(IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_SUBLABEL);
  return footer;
}

- (TableViewHeaderFooterItem*)profileSectionHeader {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text = l10n_util::GetNSString(IDS_AUTOFILL_ADDRESSES);
  return header;
}

- (TableViewItem*)itemForProfile:
    (const autofill::AutofillProfile&)autofillProfile {
  assert(autofillProfile.record_type() ==
         autofill::AutofillProfile::LOCAL_PROFILE);
  std::string guid(autofillProfile.guid());
  NSString* title = base::SysUTF16ToNSString(
      autofillProfile.GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                              GetApplicationContext()->GetApplicationLocale()));
  NSString* subTitle = base::SysUTF16ToNSString(autofillProfile.GetInfo(
      autofill::AutofillType(autofill::ADDRESS_HOME_LINE1),
      GetApplicationContext()->GetApplicationLocale()));

  AutofillProfileItem* item =
      [[AutofillProfileItem alloc] initWithType:ItemTypeAddress];
  item.text = title;
  item.leadingDetailText = subTitle;
  item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  item.accessibilityIdentifier = title;
  item.GUID = guid;
  return item;
}

- (BOOL)localProfilesExist {
  return !_personalDataManager->GetProfiles().empty();
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAddressesSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAddressesSettingsBack"));
}

#pragma mark - SettingsRootTableViewController

- (BOOL)shouldShowEditButton {
  return YES;
}

- (BOOL)editButtonEnabled {
  return [self localProfilesExist];
}

- (void)updateUIForEditState {
  [super updateUIForEditState];
  [self setSwitchItemEnabled:!self.tableView.editing
                    itemType:ItemTypeAutofillAddressSwitch];
}

// Override.
- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  // If there are no index paths, return early. This can happen if the user
  // presses the Delete button twice in quick succession.
  if (![indexPaths count])
    return;

  _deletionInProgress = YES;
  [self willDeleteItemsAtIndexPaths:indexPaths];
  // Call super to delete the items in the table view.
  [super deleteItems:indexPaths];

  // TODO(crbug.com/650390) Generalize removing empty sections
  [self removeSectionIfEmptyForSectionWithIdentifier:SectionIdentifierProfiles];
}

#pragma mark - UITableViewDelegate

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  [super setEditing:editing animated:animated];

  [self updateUIForEditState];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  // Edit mode is the state where the user can select and delete entries. In
  // edit mode, selection is handled by the superclass. When not in edit mode
  // selection presents the editing controller for the selected entry.
  if ([self.tableView isEditing]) {
    return;
  }

  TableViewModel* model = self.tableViewModel;
  if ([model itemTypeForIndexPath:indexPath] != ItemTypeAddress)
    return;

  const std::vector<autofill::AutofillProfile*> autofillProfiles =
      _personalDataManager->GetProfiles();
  AutofillProfileEditTableViewController* controller =
      [AutofillProfileEditTableViewController
          controllerWithProfile:*autofillProfiles[indexPath.item]
            personalDataManager:_personalDataManager];
  controller.dispatcher = self.dispatcher;
  [self.navigationController pushViewController:controller animated:YES];
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - Actions

// Called when the user clicks on the information button of the managed
// setting's UI. Shows a textual bubble with the information of the enterprise.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc] initWithEnterpriseName:nil];
  bubbleViewController.delegate = self;
  [self presentViewController:bubbleViewController animated:YES completion:nil];

  // Disable the button when showing the bubble.
  buttonView.enabled = NO;

  // Set the anchor and arrow direction of the bubble.
  bubbleViewController.popoverPresentationController.sourceView = buttonView;
  bubbleViewController.popoverPresentationController.sourceRect =
      buttonView.bounds;
  bubbleViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;
}

#pragma mark - UITableViewDataSource

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  // Only profile data cells are editable.
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  return [item isKindOfClass:[AutofillProfileItem class]];
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (editingStyle != UITableViewCellEditingStyleDelete)
    return;
  [self deleteItems:@[ indexPath ]];
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  switch (static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath])) {
    case ItemTypeAddress:
    case ItemTypeHeader:
    case ItemTypeFooter:
      break;
    case ItemTypeAutofillAddressSwitch: {
      SettingsSwitchCell* switchCell =
          base::mac::ObjCCastStrict<SettingsSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(autofillAddressSwitchChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeAutofillAddressManaged: {
      TableViewInfoButtonCell* managedCell =
          base::mac::ObjCCastStrict<TableViewInfoButtonCell>(cell);
      [managedCell.trailingButton
                 addTarget:self
                    action:@selector(didTapManagedUIInfoButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
  }

  return cell;
}

#pragma mark - Switch Callbacks

- (void)autofillAddressSwitchChanged:(UISwitch*)switchView {
  [self setSwitchItemOn:[switchView isOn]
               itemType:ItemTypeAutofillAddressSwitch];
  [self setAutofillProfileEnabled:[switchView isOn]];
}

#pragma mark - Switch Helpers

// Sets switchItem's state to |on|. It is important that there is only one item
// of |switchItemType| in SectionIdentifierSwitches.
- (void)setSwitchItemOn:(BOOL)on itemType:(ItemType)switchItemType {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:switchItemType
                              sectionIdentifier:SectionIdentifierSwitches];
  SettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<SettingsSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  switchItem.on = on;
}

// Sets switchItem's enabled status to |enabled| and reconfigures the
// corresponding cell. It is important that there is no more than one item of
// |switchItemType| in SectionIdentifierSwitches.
- (void)setSwitchItemEnabled:(BOOL)enabled itemType:(ItemType)switchItemType {
  TableViewModel* model = self.tableViewModel;

  if (![model hasItemForItemType:switchItemType
               sectionIdentifier:SectionIdentifierSwitches]) {
    return;
  }
  NSIndexPath* switchPath =
      [model indexPathForItemType:switchItemType
                sectionIdentifier:SectionIdentifierSwitches];
  SettingsSwitchItem* switchItem =
      base::mac::ObjCCastStrict<SettingsSwitchItem>(
          [model itemAtIndexPath:switchPath]);
  [switchItem setEnabled:enabled];
  [self reconfigureCellsForItems:@[ switchItem ]];
}

#pragma mark - PersonalDataManagerObserver

- (void)onPersonalDataChanged {
  if (_deletionInProgress)
    return;

  if ([self.tableView isEditing]) {
    // Turn off edit mode.
    [self setEditing:NO animated:NO];
  }

  [self updateUIForEditState];
  [self reloadData];
}

#pragma mark - Getters and Setter

- (BOOL)isAutofillProfileEnabled {
  return autofill::prefs::IsAutofillProfileEnabled(_browserState->GetPrefs());
}

- (void)setAutofillProfileEnabled:(BOOL)isEnabled {
  return autofill::prefs::SetAutofillProfileEnabled(_browserState->GetPrefs(),
                                                    isEnabled);
}

#pragma mark - Private

// Removes the item from the personal data manager model.
- (void)willDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  for (NSIndexPath* indexPath in indexPaths) {
    AutofillProfileItem* item = base::mac::ObjCCastStrict<AutofillProfileItem>(
        [self.tableViewModel itemAtIndexPath:indexPath]);
    _personalDataManager->RemoveByGUID([item GUID]);
  }
}

// Remove the section from the model and collectionView if there are no more
// items in the section.
- (void)removeSectionIfEmptyForSectionWithIdentifier:
    (SectionIdentifier)sectionIdentifier {
  if (![self.tableViewModel hasSectionForSectionIdentifier:sectionIdentifier]) {
    _deletionInProgress = NO;
    return;
  }
  NSInteger section =
      [self.tableViewModel sectionForSectionIdentifier:sectionIdentifier];
  if ([self.tableView numberOfRowsInSection:section] == 0) {
    // Avoid reference cycle in block.
    __weak AutofillProfileTableViewController* weakSelf = self;
    [self.tableView
        performBatchUpdates:^{
          // Obtain strong reference again.
          AutofillProfileTableViewController* strongSelf = weakSelf;
          if (!strongSelf) {
            return;
          }

          // Remove section from model and collectionView.
          [[strongSelf tableViewModel]
              removeSectionWithIdentifier:sectionIdentifier];
          [[strongSelf tableView]
                deleteSections:[NSIndexSet indexSetWithIndex:section]
              withRowAnimation:UITableViewRowAnimationAutomatic];
        }
        completion:^(BOOL finished) {
          // Obtain strong reference again.
          AutofillProfileTableViewController* strongSelf = weakSelf;
          if (!strongSelf) {
            return;
          }

          // Turn off edit mode if there is nothing to edit.
          if (![strongSelf localProfilesExist] &&
              [strongSelf.tableView isEditing]) {
            [strongSelf setEditing:NO animated:YES];
          }
          [strongSelf updateUIForEditState];
          strongSelf->_deletionInProgress = NO;
        }];
  } else {
    _deletionInProgress = NO;
  }
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  GURL convertedURL = net::GURLWithNSURL(URL);
  [self view:nil didTapLinkURL:convertedURL];
}

@end
