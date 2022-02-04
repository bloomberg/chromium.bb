// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/handoff_table_view_controller.h"

#import "base/mac/foundation_util.h"
#include "components/handoff/pref_names_ios.h"
#include "components/prefs/pref_member.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSwitch = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSwitch = kItemTypeEnumZero,
  ItemTypeFooter,
};

}  // namespace

@interface HandoffTableViewController () <BooleanObserver> {
  // Pref for whether Handoff is enabled.
  PrefBackedBoolean* _handoffEnabled;

  // Item for displaying handoff switch.
  TableViewSwitchItem* _handoffSwitchItem;
}

@end

@implementation HandoffTableViewController

#pragma mark - Initialization

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  DCHECK(browserState);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_OPTIONS_CONTINUITY_LABEL);
    _handoffEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:browserState->GetPrefs()
                   prefName:prefs::kIosHandoffToOtherDevices];
    [_handoffEnabled setObserver:self];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.estimatedSectionFooterHeight = 70;

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSwitch];
  _handoffSwitchItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeSwitch];
  _handoffSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES);
  _handoffSwitchItem.on = _handoffEnabled.value;
  [model addItem:_handoffSwitchItem
      toSectionWithIdentifier:SectionIdentifierSwitch];

  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footer.text = l10n_util::GetNSString(
      IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES_DETAILS);
  [model setFooter:footer forSectionWithIdentifier:SectionIdentifierSwitch];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  if (itemType == ItemTypeSwitch) {
    TableViewSwitchCell* switchCell =
        base::mac::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchChanged:)
                    forControlEvents:UIControlEventValueChanged];
  }
  return cell;
}

#pragma mark - Private

- (void)switchChanged:(UISwitch*)switchView {
  _handoffEnabled.value = switchView.isOn;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  // Update the cell.
  _handoffSwitchItem.on = _handoffEnabled.value;
  [self reconfigureCellsForItems:@[ _handoffSwitchItem ]];
}

@end
