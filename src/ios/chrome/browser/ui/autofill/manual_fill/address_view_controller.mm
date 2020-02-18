// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/address_view_controller.h"

#include "base/ios/ios_util.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace manual_fill {

NSString* const AddressTableViewAccessibilityIdentifier =
    @"kManualFillAddressTableViewAccessibilityIdentifier";

}  // namespace manual_fill

@implementation AddressViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier =
      manual_fill::AddressTableViewAccessibilityIdentifier;
}

#pragma mark - ManualFillAddressConsumer

// TODO(crbug.com/845472): look at replacing ManualFillXXXConsumer with
// ManualFillItemsConsumer.
- (void)presentAddresses:(NSArray<ManualFillAddressItem*>*)addresses {
  UMA_HISTOGRAM_COUNTS_100("ManualFallback.PresentedOptions.Profiles",
                           addresses.count);
  [self presentDataItems:(NSArray<TableViewItem*>*)addresses];
}

- (void)presentActions:(NSArray<ManualFillActionItem*>*)actions {
  [self presentActionItems:actions];
}

@end
