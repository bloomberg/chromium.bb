// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/table_cell_catalog_view_controller.h"

#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/settings/cells/autofill_data_item.h"
#import "ios/chrome/browser/ui/settings/cells/encryption_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_detail_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_accessory_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierText = kSectionIdentifierEnumZero,
  SectionIdentifierSettings,
  SectionIdentifierAutofill,
  SectionIdentifierURL,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeText = kItemTypeEnumZero,
  ItemTypeTextHeader,
  ItemTypeTextFooter,
  ItemTypeTextButton,
  ItemTypeURLNoMetadata,
  ItemTypeTextAccessoryImage,
  ItemTypeTextAccessoryNoImage,
  ItemTypeURLWithTimestamp,
  ItemTypeURLWithSize,
  ItemTypeURLWithSupplementalText,
  ItemTypeURLWithBadgeImage,
  ItemTypeTextSettingsDetail,
  ItemTypeEncryption,
  ItemTypeLinkFooter,
  ItemTypeDetailText,
  ItemTypeSettingsSwitch,
  ItemTypeAutofillEditItem,
  ItemTypeAutofillData,
};
}

@implementation TableCellCatalogViewController

- (instancetype)init {
  if ((self = [super
           initWithTableViewStyle:UITableViewStyleGrouped
                      appBarStyle:ChromeTableViewControllerStyleWithAppBar])) {
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Table Cell Catalog";
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = 56.0;
  self.tableView.estimatedSectionHeaderHeight = 56.0;
  self.tableView.estimatedSectionFooterHeight = 56.0;

  [self loadModel];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierText];
  [model addSectionWithIdentifier:SectionIdentifierSettings];
  [model addSectionWithIdentifier:SectionIdentifierAutofill];
  [model addSectionWithIdentifier:SectionIdentifierURL];

  // SectionIdentifierText.
  TableViewTextHeaderFooterItem* textHeaderFooterItem =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeTextHeader];
  textHeaderFooterItem.text = @"Simple Text Header";
  [model setHeader:textHeaderFooterItem
      forSectionWithIdentifier:SectionIdentifierText];

  TableViewTextItem* textItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeText];
  textItem.text = @"Simple Text Cell";
  textItem.textAlignment = NSTextAlignmentCenter;
  textItem.textColor = [UIColor blackColor];
  [model addItem:textItem toSectionWithIdentifier:SectionIdentifierText];

  textItem = [[TableViewTextItem alloc] initWithType:ItemTypeText];
  textItem.text = @"1234";
  textItem.masked = YES;
  [model addItem:textItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewAccessoryItem* textAccessoryItem =
      [[TableViewAccessoryItem alloc] initWithType:ItemTypeTextAccessoryImage];
  textAccessoryItem.title = @"Text Accessory with History Image";
  textAccessoryItem.image = [UIImage imageNamed:@"show_history"];
  [model addItem:textAccessoryItem
      toSectionWithIdentifier:SectionIdentifierText];

  textAccessoryItem = [[TableViewAccessoryItem alloc]
      initWithType:ItemTypeTextAccessoryNoImage];
  textAccessoryItem.title = @"Text Accessory No Image";
  [model addItem:textAccessoryItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextItem* textItemDefault =
      [[TableViewTextItem alloc] initWithType:ItemTypeText];
  textItemDefault.text = @"Simple Text Cell with Defaults";
  [model addItem:textItemDefault toSectionWithIdentifier:SectionIdentifierText];

  textHeaderFooterItem =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeTextFooter];
  textHeaderFooterItem.text = @"Simple Text Footer";
  [model setFooter:textHeaderFooterItem
      forSectionWithIdentifier:SectionIdentifierText];

  TableViewTextButtonItem* textActionButtonItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeTextButton];
  textActionButtonItem.text = @"Hello, you should do something.";
  textActionButtonItem.buttonText = @"Do something";
  [model addItem:textActionButtonItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewDetailTextItem* detailTextItem =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeDetailText];
  detailTextItem.text = @"Item with two labels";
  detailTextItem.detailText =
      @"The second label is optional and is mostly displayed on one line";
  [model addItem:detailTextItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewDetailTextItem* noDetailTextItem =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeDetailText];
  noDetailTextItem.text = @"Detail item on one line.";
  [model addItem:noDetailTextItem
      toSectionWithIdentifier:SectionIdentifierText];

  // SectionIdentifierSettings.
  TableViewTextHeaderFooterItem* settingsHeader =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeTextHeader];
  settingsHeader.text = @"Settings";
  [model setHeader:settingsHeader
      forSectionWithIdentifier:SectionIdentifierSettings];

  SettingsDetailItem* settingsDetailItem =
      [[SettingsDetailItem alloc] initWithType:ItemTypeTextSettingsDetail];
  settingsDetailItem.text = @"Settings cells";
  settingsDetailItem.detailText = @"Short";
  [model addItem:settingsDetailItem
      toSectionWithIdentifier:SectionIdentifierSettings];

  SettingsDetailItem* settingsDetailItemLong =
      [[SettingsDetailItem alloc] initWithType:ItemTypeTextSettingsDetail];
  settingsDetailItemLong.text = @"Very long text eating the other detail label";
  settingsDetailItemLong.detailText = @"A bit less short";
  [model addItem:settingsDetailItemLong
      toSectionWithIdentifier:SectionIdentifierSettings];

  SettingsSwitchItem* settingsSwitchItem =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeSettingsSwitch];
  settingsSwitchItem.text = @"This is a switch item";
  [model addItem:settingsSwitchItem
      toSectionWithIdentifier:SectionIdentifierSettings];

  EncryptionItem* encryptionChecked =
      [[EncryptionItem alloc] initWithType:ItemTypeEncryption];
  encryptionChecked.text =
      @"These two cells have exactly the same text, but one has a checkmark "
      @"and the other does not.  They should lay out identically, and the "
      @"presence of the checkmark should not cause the text to reflow.";
  encryptionChecked.accessoryType = UITableViewCellAccessoryCheckmark;
  [model addItem:encryptionChecked
      toSectionWithIdentifier:SectionIdentifierSettings];

  EncryptionItem* encryptionUnchecked =
      [[EncryptionItem alloc] initWithType:ItemTypeEncryption];
  encryptionUnchecked.text =
      @"These two cells have exactly the same text, but one has a checkmark "
      @"and the other does not.  They should lay out identically, and the "
      @"presence of the checkmark should not cause the text to reflow.";
  [model addItem:encryptionUnchecked
      toSectionWithIdentifier:SectionIdentifierSettings];

  TableViewLinkHeaderFooterItem* linkFooter =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeLinkFooter];
  linkFooter.text =
      @"This is a footer text view with a BEGIN_LINKlinkEND_LINK in the middle";
  [model setFooter:linkFooter
      forSectionWithIdentifier:SectionIdentifierSettings];

  // SectionIdentifierAutofill.
  TableViewTextHeaderFooterItem* autofillHeader =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeTextHeader];
  autofillHeader.text = @"Autofill";
  [model setHeader:autofillHeader
      forSectionWithIdentifier:SectionIdentifierAutofill];

  AutofillEditItem* autofillEditItem =
      [[AutofillEditItem alloc] initWithType:ItemTypeAutofillEditItem];
  autofillEditItem.textFieldName = @"Autofill field";
  autofillEditItem.textFieldValue = @" with a value";
  autofillEditItem.identifyingIcon =
      [UIImage imageNamed:@"table_view_cell_check_mark"];
  [model addItem:autofillEditItem
      toSectionWithIdentifier:SectionIdentifierAutofill];

  AutofillDataItem* autofillItemWithMainLeading =
      [[AutofillDataItem alloc] initWithType:ItemTypeAutofillData];
  autofillItemWithMainLeading.text = @"Main Text";
  autofillItemWithMainLeading.trailingDetailText = @"Trailing Detail Text";
  [model addItem:autofillItemWithMainLeading
      toSectionWithIdentifier:SectionIdentifierAutofill];

  AutofillDataItem* autofillItemWithLeading =
      [[AutofillDataItem alloc] initWithType:ItemTypeAutofillData];
  autofillItemWithLeading.text = @"Main Text";
  autofillItemWithLeading.leadingDetailText = @"Leading Detail Text";
  autofillItemWithLeading.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  [model addItem:autofillItemWithLeading
      toSectionWithIdentifier:SectionIdentifierAutofill];

  AutofillDataItem* autofillItemWithAllTexts =
      [[AutofillDataItem alloc] initWithType:ItemTypeAutofillData];
  autofillItemWithAllTexts.text = @"Main Text";
  autofillItemWithAllTexts.leadingDetailText = @"Leading Detail Text";
  autofillItemWithAllTexts.trailingDetailText = @"Trailing Detail Text";
  autofillItemWithAllTexts.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  [model addItem:autofillItemWithAllTexts
      toSectionWithIdentifier:SectionIdentifierAutofill];

  // SectionIdentifierURL.
  TableViewURLItem* item =
      [[TableViewURLItem alloc] initWithType:ItemTypeURLNoMetadata];
  item.title = @"Google Design";
  item.URL = GURL("https://design.google.com");
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLNoMetadata];
  item.URL = GURL("https://notitle.google.com");
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithTimestamp];
  item.title = @"Google";
  item.URL = GURL("https://www.google.com");
  item.metadata = @"3:42 PM";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithSize];
  item.title = @"World Series 2017: Houston Astros Defeat Someone Else";
  item.URL = GURL("https://m.bbc.com");
  item.metadata = @"176 KB";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item =
      [[TableViewURLItem alloc] initWithType:ItemTypeURLWithSupplementalText];
  item.title = @"Chrome | Google Blog";
  item.URL = GURL("https://blog.google/products/chrome/");
  item.supplementalURLText = @"Read 4 days ago";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithBadgeImage];
  item.title = @"Photos - Google Photos";
  item.URL = GURL("https://photos.google.com/");
  item.badgeImage = [UIImage imageNamed:@"table_view_cell_check_mark"];
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];
}

@end
