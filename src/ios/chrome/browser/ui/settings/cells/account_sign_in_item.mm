// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/account_sign_in_item.h"

#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Account profile picture size.
const CGFloat kAccountProfilePhotoDimension = 40.0f;

}  // namespace

@implementation AccountSignInItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsImageDetailTextCell class];
    self.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  return self;
}

#pragma mark - CollectionViewItem

- (void)configureCell:(SettingsImageDetailTextCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text =
      l10n_util::GetNSString(IDS_IOS_SIGN_IN_TO_CHROME_SETTING_TITLE);
  cell.detailTextLabel.text = self.detailText;
  cell.image = CircularImageFromImage(ios::GetChromeBrowserProvider()
                                          ->GetSigninResourcesProvider()
                                          ->GetDefaultAvatar(),
                                      kAccountProfilePhotoDimension);
}

@end
