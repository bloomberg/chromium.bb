// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"

#include "base/ios/ios_util.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/action_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_cell.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace manual_fill {

NSString* const PasswordSearchBarAccessibilityIdentifier =
    @"kManualFillPasswordSearchBarAccessibilityIdentifier";
NSString* const PasswordTableViewAccessibilityIdentifier =
    @"kManualFillPasswordTableViewAccessibilityIdentifier";

}  // namespace manual_fill

@interface PasswordViewController ()

// Search controller if any.
@property(nonatomic, strong) UISearchController* searchController;

@end

@implementation PasswordViewController

- (instancetype)initWithSearchController:(UISearchController*)searchController {
  self = [super init];
  if (self) {
    _searchController = searchController;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier =
      manual_fill::PasswordTableViewAccessibilityIdentifier;

  self.definesPresentationContext = YES;
  self.searchController.searchBar.backgroundColor = [UIColor clearColor];
  self.searchController.obscuresBackgroundDuringPresentation = NO;
  self.navigationItem.searchController = self.searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;
  self.searchController.searchBar.accessibilityIdentifier =
      manual_fill::PasswordSearchBarAccessibilityIdentifier;
  NSString* titleString =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_USE_OTHER_PASSWORD);
  self.title = titleString;

  // Center search bar vertically so it looks centered in the header when
  // searching.  The cancel button is centered / decentered on
  // viewWillAppear and viewDidDisappear.
  UIOffset offset =
      UIOffsetMake(0.0f, kTableViewNavigationVerticalOffsetForSearchHeader);
  self.searchController.searchBar.searchFieldBackgroundPositionAdjustment =
      offset;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Center search bar's cancel button vertically so it looks centered.
  // We change the cancel button proxy styles, so we will return it to
  // default in viewDidDisappear.
  if (self.searchController) {
    UIOffset offset =
        UIOffsetMake(0.0f, kTableViewNavigationVerticalOffsetForSearchHeader);
    UIBarButtonItem* cancelButton = [UIBarButtonItem
        appearanceWhenContainedInInstancesOfClasses:@ [[UISearchBar class]]];
    [cancelButton setTitlePositionAdjustment:offset
                               forBarMetrics:UIBarMetricsDefault];
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];

  // Restore to default origin offset for cancel button proxy style.
  if (self.searchController) {
    UIBarButtonItem* cancelButton = [UIBarButtonItem
        appearanceWhenContainedInInstancesOfClasses:@ [[UISearchBar class]]];
    [cancelButton setTitlePositionAdjustment:UIOffsetZero
                               forBarMetrics:UIBarMetricsDefault];
  }
}

#pragma mark - ManualFillPasswordConsumer

- (void)presentCredentials:(NSArray<ManualFillCredentialItem*>*)credentials {
  if (self.searchController) {
    UMA_HISTOGRAM_COUNTS_1000("ManualFallback.PresentedOptions.AllPasswords",
                              credentials.count);
  } else {
    UMA_HISTOGRAM_COUNTS_100("ManualFallback.PresentedOptions.Passwords",
                             credentials.count);
  }
  // If no items were posted and there is no search bar, present the empty item
  // and return.
  if (!credentials.count && !self.searchController) {
    ManualFillActionItem* emptyCredentialItem = [[ManualFillActionItem alloc]
        initWithTitle:l10n_util::GetNSString(
                          IDS_IOS_MANUAL_FALLBACK_NO_PASSWORDS_FOR_SITE)
               action:nil];
    emptyCredentialItem.enabled = NO;
    emptyCredentialItem.showSeparator = YES;
    [self presentDataItems:@[ emptyCredentialItem ]];
    return;
  }

  [self presentDataItems:credentials];
}

- (void)presentActions:(NSArray<ManualFillActionItem*>*)actions {
  [self presentActionItems:actions];
}

@end
