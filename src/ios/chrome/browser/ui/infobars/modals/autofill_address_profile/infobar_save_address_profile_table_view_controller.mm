// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_save_address_profile_table_view_controller.h"

#include "base/feature_list.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_save_address_profile_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSaveModalFields = kSectionIdentifierEnumZero,
  SectionIdentifierUpdateModalFields,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSaveModalFields = kItemTypeEnumZero,
  ItemTypeAddressProfileSaveUpdateButton,
  ItemTypeUpdateModalDescription,
  ItemTypeUpdateModalTitle,
  ItemTypeUpdateModalFields
};

@interface InfobarSaveAddressProfileTableViewController ()

// InfobarSaveAddressProfileModalDelegate for this ViewController.
@property(nonatomic, strong) id<InfobarSaveAddressProfileModalDelegate>
    saveAddressProfileModalDelegate;
// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;

// Item for displaying and editing the address.
@property(nonatomic, copy) NSString* address;
// Item for displaying and editing the phone number.
@property(nonatomic, copy) NSString* phoneNumber;
// Item for displaying and editing the email address.
@property(nonatomic, copy) NSString* emailAddress;
// YES if the Address Profile being displayed has been saved.
@property(nonatomic, assign) BOOL currentAddressProfileSaved;
// Yes, if the update address profile modal is to be displayed.
@property(nonatomic, assign) BOOL isUpdateModal;
// Contains the content for the update modal.
@property(nonatomic, copy) NSDictionary* profileDataDiff;
// Description of the update modal.
@property(nonatomic, copy) NSString* updateModalDescription;

@end

@implementation InfobarSaveAddressProfileTableViewController

- (instancetype)initWithModalDelegate:
    (id<InfobarSaveAddressProfileModalDelegate>)modalDelegate {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _saveAddressProfileModalDelegate = modalDelegate;
    _metricsRecorder = [[InfobarMetricsRecorder alloc]
        initWithType:InfobarType::kInfobarTypeSaveAutofillAddressProfile];
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.styler.tableViewBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 0;
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(dismissInfobarModal)];
  cancelButton.accessibilityIdentifier = kInfobarModalCancelButton;
  self.navigationItem.leftBarButtonItem = cancelButton;

  if (!self.currentAddressProfileSaved) {
    UIBarButtonItem* editButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemEdit
                             target:self
                             action:@selector(showEditAddressProfileModal)];
    // TODO(crbug.com/1167062): Add accessibility identifier for the edit
    // button.
    self.navigationItem.rightBarButtonItem = editButton;
  }

  self.navigationController.navigationBar.prefersLargeTitles = NO;

  if (self.isUpdateModal) {
    self.navigationItem.title =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE);
  } else {
    self.navigationItem.title =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
  }

  [self loadModel];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Presented];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Dismissed];
  [super viewDidDisappear:animated];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  self.tableView.scrollEnabled =
      self.tableView.contentSize.height > self.view.frame.size.height;
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  if (self.isUpdateModal) {
    [self loadUpdateAddressModal];
  } else {
    [self loadSaveAddressModal];
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  if (itemType == ItemTypeAddressProfileSaveUpdateButton) {
    TableViewTextButtonCell* tableViewTextButtonCell =
        base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
    [tableViewTextButtonCell.button
               addTarget:self
                  action:@selector(saveAddressProfileButtonWasPressed:)
        forControlEvents:UIControlEventTouchUpInside];
  } else {
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
  }
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];
  if (base::FeatureList::IsEnabled(
          autofill::features::
              kAutofillAddressProfileSavePromptAddressVerificationSupport)) {
    if (itemType == ItemTypeSaveModalFields ||
        itemType == ItemTypeUpdateModalFields) {
      [self ensureContextMenuShownForItemType:itemType atIndexPath:indexPath];
    }
  }
}

// If the context menu is not shown for a given item type, constructs that
// menu and shows it. This method should only be called for item types
// representing the cells with the save/update address profile modal.
- (void)ensureContextMenuShownForItemType:(NSInteger)itemType
                              atIndexPath:(NSIndexPath*)indexPath {
  UIMenuController* menu = [UIMenuController sharedMenuController];
  if (![menu isMenuVisible]) {
    menu.menuItems = [self menuItems];
    [self becomeFirstResponder];
#if !defined(__IPHONE_13_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_13_0
    [menu setTargetRect:[self.tableView rectForRowAtIndexPath:indexPath]
                 inView:self.tableView];
    [menu setMenuVisible:YES animated:YES];
#else
    [menu showMenuFromView:self.tableView
                      rect:[self.tableView rectForRowAtIndexPath:indexPath]];
#endif
  }
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (action == @selector(showEditAddressProfileModal)) {
    return YES;
  }
  return NO;
}

#pragma mark - InfobarSaveAddressProfileModalConsumer

- (void)setupModalViewControllerWithPrefs:(NSDictionary*)prefs {
  self.address = prefs[kAddressPrefKey];
  self.phoneNumber = prefs[kPhonePrefKey];
  self.emailAddress = prefs[kEmailPrefKey];
  self.currentAddressProfileSaved =
      [prefs[kCurrentAddressProfileSavedPrefKey] boolValue];
  self.isUpdateModal = [prefs[kIsUpdateModalPrefKey] boolValue];
  self.profileDataDiff = prefs[kProfileDataDiffKey];
  self.updateModalDescription = prefs[kUpdateModalDescriptionKey];
  [self.tableView reloadData];
}

#pragma mark - Actions

- (void)saveAddressProfileButtonWasPressed:(UIButton*)sender {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalAcceptedTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Accepted];
  [self.saveAddressProfileModalDelegate modalInfobarButtonWasAccepted:self];
}

- (void)dismissInfobarModal {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalCancelledTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Canceled];
  [self.saveAddressProfileModalDelegate dismissInfobarModal:self];
}

- (void)showEditAddressProfileModal {
  [self.saveAddressProfileModalDelegate showEditView];
}

#pragma mark - Private Methods

- (void)loadUpdateAddressModal {
  DCHECK([self.profileDataDiff count] > 0);

  // Determines whether the old section is to be shown or not.
  BOOL showOld = NO;
  for (NSNumber* type in self.profileDataDiff) {
    if ([self.profileDataDiff[type][1] length] > 0) {
      showOld = YES;
    }
  }

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierUpdateModalFields];
  [model addItem:[self updateModalDescriptionItem]
      toSectionWithIdentifier:SectionIdentifierUpdateModalFields];

  if (showOld) {
    TableViewTextItem* newTitleItem = [self
        titleWithTextItem:
            l10n_util::GetNSString(
                IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_NEW_VALUES_SECTION_LABEL)];
    [model addItem:newTitleItem
        toSectionWithIdentifier:SectionIdentifierUpdateModalFields];
  }

  // Store the last added field to the modal other than the update button.
  SettingsImageDetailTextItem* lastAddedItem = nil;

  for (NSNumber* type in self.profileDataDiff) {
    if ([self.profileDataDiff[type][0] length] > 0) {
      SettingsImageDetailTextItem* newItem =
          [self detailItemWithType:ItemTypeUpdateModalFields
                              text:self.profileDataDiff[type][0]
                     iconImageName:[self iconForAutofillInputTypeNumber:type]
              imageTintColorIsGrey:NO];
      lastAddedItem = newItem;
      [model addItem:newItem
          toSectionWithIdentifier:SectionIdentifierUpdateModalFields];
    }
  }

  if (showOld) {
    TableViewTextItem* oldTitleItem = [self
        titleWithTextItem:
            l10n_util::GetNSString(
                IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OLD_VALUES_SECTION_LABEL)];
    [model addItem:oldTitleItem
        toSectionWithIdentifier:SectionIdentifierUpdateModalFields];
    for (NSNumber* type in self.profileDataDiff) {
      if ([self.profileDataDiff[type][1] length] > 0) {
        SettingsImageDetailTextItem* oldItem =
            [self detailItemWithType:ItemTypeUpdateModalFields
                                text:self.profileDataDiff[type][1]
                       iconImageName:[self iconForAutofillInputTypeNumber:type]
                imageTintColorIsGrey:YES];
        lastAddedItem = oldItem;
        [model addItem:oldItem
            toSectionWithIdentifier:SectionIdentifierUpdateModalFields];
      }
    }
  }

  // Remove the separator after the last field.
  lastAddedItem.useCustomSeparator = NO;

  [model addItem:[self saveUpdateButton]
      toSectionWithIdentifier:SectionIdentifierUpdateModalFields];
}

- (void)loadSaveAddressModal {
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSaveModalFields];

  SettingsImageDetailTextItem* addressItem = [self
      detailItemForSaveModalWithText:self.address
                      autofillUIType:AutofillUITypeProfileHomeAddressStreet];
  [model addItem:addressItem
      toSectionWithIdentifier:SectionIdentifierSaveModalFields];

  // Store the last added field to the modal other than the save button.
  SettingsImageDetailTextItem* lastAddedItem = addressItem;

  if ([self.emailAddress length]) {
    SettingsImageDetailTextItem* emailItem =
        [self detailItemForSaveModalWithText:self.emailAddress
                              autofillUIType:AutofillUITypeProfileEmailAddress];
    [model addItem:emailItem
        toSectionWithIdentifier:SectionIdentifierSaveModalFields];
    lastAddedItem = emailItem;
  }

  if ([self.phoneNumber length]) {
    SettingsImageDetailTextItem* phoneItem =
        [self detailItemForSaveModalWithText:self.phoneNumber
                              autofillUIType:
                                  AutofillUITypeProfileHomePhoneWholeNumber];
    [model addItem:phoneItem
        toSectionWithIdentifier:SectionIdentifierSaveModalFields];
    lastAddedItem = phoneItem;
  }

  // Remove the separator after the last field.
  lastAddedItem.useCustomSeparator = NO;

  [model addItem:[self saveUpdateButton]
      toSectionWithIdentifier:SectionIdentifierSaveModalFields];
}

- (TableViewTextButtonItem*)saveUpdateButton {
  TableViewTextButtonItem* saveUpdateButton = [[TableViewTextButtonItem alloc]
      initWithType:ItemTypeAddressProfileSaveUpdateButton];
  saveUpdateButton.textAlignment = NSTextAlignmentNatural;

  if (self.isUpdateModal) {
    saveUpdateButton.buttonText = l10n_util::GetNSString(
        IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
  } else {
    saveUpdateButton.buttonText = l10n_util::GetNSString(
        IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
  }

  saveUpdateButton.enabled = !self.currentAddressProfileSaved;
  saveUpdateButton.disableButtonIntrinsicWidth = YES;
  return saveUpdateButton;
}

- (TableViewTextItem*)titleWithTextItem:(NSString*)text {
  TableViewTextItem* titleItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeUpdateModalTitle];
  titleItem.text = text;
  titleItem.textFont =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  return titleItem;
}

- (TableViewTextItem*)updateModalDescriptionItem {
  TableViewTextItem* descriptionItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeUpdateModalDescription];
  descriptionItem.text = self.updateModalDescription;
  descriptionItem.textFont =
      [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
  descriptionItem.textColor = UIColor.cr_secondaryLabelColor;
  return descriptionItem;
}

- (NSString*)iconForAutofillUIType:(AutofillUIType)type {
  switch (type) {
    case AutofillUITypeNameFullWithHonorificPrefix:
      return @"infobar_profile_icon";
    case AutofillUITypeAddressHomeAddress:
    case AutofillUITypeProfileHomeAddressStreet:
      return @"infobar_autofill_address_icon";
    case AutofillUITypeProfileEmailAddress:
      return @"infobar_email_icon";
    case AutofillUITypeProfileHomePhoneWholeNumber:
      return @"infobar_phone_icon";
    default:
      NOTREACHED();
      return @"";
  }
}

- (NSString*)iconForAutofillInputTypeNumber:(NSNumber*)val {
  return [self iconForAutofillUIType:(AutofillUIType)[val intValue]];
}

// Returns an array of UIMenuItems to display in a context menu on the site
// cell.
- (NSArray*)menuItems {
  // TODO(crbug.com/1167062): Use proper i18n string for Edit.
  UIMenuItem* editOption =
      [[UIMenuItem alloc] initWithTitle:@"Edit"
                                 action:@selector(showEditAddressProfileModal)];
  return @[ editOption ];
}

#pragma mark - Item Constructors

// Returns a |SettingsImageDetailTextItem| for the fields to be shown in the
// save address modal.
- (SettingsImageDetailTextItem*)
    detailItemForSaveModalWithText:(NSString*)text
                    autofillUIType:(AutofillUIType)autofillUIType {
  return [self detailItemWithType:ItemTypeSaveModalFields
                             text:text
                    iconImageName:[self iconForAutofillUIType:autofillUIType]
             imageTintColorIsGrey:YES];
}

- (SettingsImageDetailTextItem*)detailItemWithType:(NSInteger)type
                                              text:(NSString*)text
                                     iconImageName:(NSString*)iconImageName
                              imageTintColorIsGrey:(BOOL)imageTintColorIsGrey {
  SettingsImageDetailTextItem* detailItem =
      [[SettingsImageDetailTextItem alloc] initWithType:type];

  detailItem.text = text;
  detailItem.alignImageWithFirstLineOfText = YES;
  if ([iconImageName length]) {
    detailItem.image = [[UIImage imageNamed:iconImageName]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    detailItem.useCustomSeparator = YES;
    if (imageTintColorIsGrey) {
      detailItem.imageViewTintColor = [UIColor colorNamed:kGrey400Color];
    } else {
      detailItem.imageViewTintColor = [UIColor colorNamed:kBlueColor];
    }
  }

  return detailItem;
}

@end
