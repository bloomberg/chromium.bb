// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_password_table_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_password_modal_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeURL = kItemTypeEnumZero,
  ItemTypeUsername,
  ItemTypePassword,
  ItemTypeSaveCredentials,
};

@interface InfobarPasswordTableViewController ()
// Item that holds the Username TextField information.
@property(nonatomic, strong) TableViewTextEditItem* usernameItem;
// Item that holds the Password TextField information.
@property(nonatomic, strong) TableViewTextEditItem* passwordItem;
// Item that holds the SaveCredentials Button information.
@property(nonatomic, strong) TableViewTextButtonItem* saveCredentialsItem;
@end

@implementation InfobarPasswordTableViewController

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor whiteColor];
  self.styler.cellBackgroundColor = [UIColor whiteColor];
  self.tableView.scrollEnabled = NO;
  self.tableView.sectionHeaderHeight = 0;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(dismissInfobarModal:)];
  cancelButton.accessibilityIdentifier = kInfobarModalCancelButton;
  UIImage* settingsImage = [[UIImage imageNamed:@"infobar_settings_icon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  UIBarButtonItem* settingsButton =
      [[UIBarButtonItem alloc] initWithImage:settingsImage
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:nil];
  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationItem.rightBarButtonItem = settingsButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  [self loadModel];
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierContent];

  TableViewDetailIconItem* URLDetailItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeURL];
  URLDetailItem.text = l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE);
  URLDetailItem.detailText = self.URL;
  [model addItem:URLDetailItem
      toSectionWithIdentifier:SectionIdentifierContent];

  self.usernameItem =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeUsername];
  self.usernameItem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
  self.usernameItem.textFieldValue = self.username;
  self.usernameItem.textFieldEnabled = YES;
  self.usernameItem.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  [model addItem:self.usernameItem
      toSectionWithIdentifier:SectionIdentifierContent];

  self.passwordItem =
      [[TableViewTextEditItem alloc] initWithType:ItemTypePassword];
  self.passwordItem.textFieldName =
      l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);
  self.passwordItem.textFieldValue = self.maskedPassword;
  self.passwordItem.textFieldEnabled = YES;
  self.passwordItem.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  [model addItem:self.passwordItem
      toSectionWithIdentifier:SectionIdentifierContent];

  self.saveCredentialsItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeSaveCredentials];
  self.saveCredentialsItem.buttonText = self.saveButtonText;
  [model addItem:self.saveCredentialsItem
      toSectionWithIdentifier:SectionIdentifierContent];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  switch (itemType) {
    case ItemTypeSaveCredentials: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(saveCredentialsButtonWasPressed:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeUsername:
    case ItemTypePassword: {
      TableViewTextEditCell* editCell =
          base::mac::ObjCCast<TableViewTextEditCell>(cell);
      [editCell.textField addTarget:self
                             action:@selector(updateSaveCredentialsButtonState)
                   forControlEvents:UIControlEventEditingChanged];
      break;
    }
    case ItemTypeURL:
      break;
  }

  return cell;
}

#pragma mark - Private Methods

- (void)updateSaveCredentialsButtonState {
  BOOL currentButtonState = [self.saveCredentialsItem isEnabled];
  BOOL newButtonState = [self.passwordItem.textFieldValue length] ? YES : NO;
  if (currentButtonState != newButtonState) {
    self.saveCredentialsItem.enabled = newButtonState;
    [self reconfigureCellsForItems:@[ self.saveCredentialsItem ]];
  }
}

- (void)dismissInfobarModal:(UIButton*)sender {
  [self.infobarModalDelegate dismissInfobarModal:sender completion:nil];
}

- (void)saveCredentialsButtonWasPressed:(UIButton*)sender {
  [self.infobarModalDelegate
      updateCredentialsWithUsername:self.usernameItem.textFieldValue
                           password:self.passwordItem.textFieldValue];
}

@end
