// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/default_browser/default_browser_settings_table_view_controller.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Icon image names.
NSString* const kSettingsImageName = @"settings";
NSString* const kSelectChromeStepImageName = @"chrome_icon";

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSteps = kSectionIdentifierEnumZero,
  SectionIdentifierOpenSettings,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeOpenSettingsStep = kItemTypeEnumZero,
  ItemTypeTapDefaultBrowserAppStep,
  ItemTypeSelectChromeStep,
  ItemTypeOpenSettingsButton,
  ItemTypeHeaderItem,
};

}  // namespace

@implementation DefaultBrowserSettingsTableViewController

- (instancetype)init {
  UITableViewStyle style = UITableViewStylePlain;
  return [super initWithStyle:style];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_DEFAULT_BROWSER);
  self.shouldHideDoneButton = YES;

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierSteps];
  TableViewLinkHeaderFooterItem* headerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeHeaderItem];
  headerItem.text = l10n_util::GetNSString(IDS_IOS_SETTINGS_HEADER_TEXT);
  [self.tableViewModel setHeader:headerItem
        forSectionWithIdentifier:SectionIdentifierSteps];

  TableViewDetailIconItem* openSettingsStepItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeOpenSettingsStep];
  openSettingsStepItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_OPEN_SETTINGS_STEP);
  openSettingsStepItem.iconImageName = kSettingsImageName;
  [self.tableViewModel addItem:openSettingsStepItem
       toSectionWithIdentifier:SectionIdentifierSteps];

  TableViewDetailIconItem* tapDefaultBrowserAppStepItem =
      [[TableViewDetailIconItem alloc]
          initWithType:ItemTypeTapDefaultBrowserAppStep];
  tapDefaultBrowserAppStepItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_TAP_DEFAULT_BROWSER_APP_STEP);
  tapDefaultBrowserAppStepItem.iconImageName = kSettingsImageName;
  [self.tableViewModel addItem:tapDefaultBrowserAppStepItem
       toSectionWithIdentifier:SectionIdentifierSteps];

  TableViewDetailIconItem* selectChromeStepItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeSelectChromeStep];
  selectChromeStepItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SELECT_CHROME_STEP);
  selectChromeStepItem.iconImageName = kSelectChromeStepImageName;
  [self.tableViewModel addItem:selectChromeStepItem
       toSectionWithIdentifier:SectionIdentifierSteps];

  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierOpenSettings];
  TableViewTextItem* openSettingsButtonItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeOpenSettingsButton];
  openSettingsButtonItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_OPEN_CHROME_SETTINGS_BUTTON_TEXT);
  openSettingsButtonItem.accessibilityTraits |= UIAccessibilityTraitButton;
  openSettingsButtonItem.textColor = [UIColor colorNamed:kBlueColor];
  [self.tableViewModel addItem:openSettingsButtonItem
       toSectionWithIdentifier:SectionIdentifierOpenSettings];
}

#pragma mark UITableViewDelegate

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  if (itemType == ItemTypeOpenSettingsStep ||
      itemType == ItemTypeTapDefaultBrowserAppStep ||
      itemType == ItemTypeSelectChromeStep) {
    [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
  }
  return cell;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  [self.tableView deselectRowAtIndexPath:indexPath animated:NO];

  if (itemType == ItemTypeOpenSettingsButton) {
    base::RecordAction(base::UserMetricsAction("Settings.DefaultBrowser"));
    [[UIApplication sharedApplication]
                  openURL:[NSURL
                              URLWithString:UIApplicationOpenSettingsURLString]
                  options:{}
        completionHandler:nil];
  }
}

@end
