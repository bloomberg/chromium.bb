// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_cell.h"
#include "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SettingsImageDetailTextItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsImageDetailTextCell class];
  }
  return self;
}

- (void)configureCell:(SettingsImageDetailTextCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  DCHECK(self.image);
  cell.image = self.image;
}

@end
