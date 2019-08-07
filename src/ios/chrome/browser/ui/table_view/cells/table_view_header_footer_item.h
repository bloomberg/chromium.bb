// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_HEADER_FOOTER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_HEADER_FOOTER_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/list_model/list_item.h"

@class ChromeTableViewStyler;

// TableViewHeaderFooterItem holds the model data for a given table view
// header/footer item.
@interface TableViewHeaderFooterItem : ListItem

// Configures the given headerFooter view with the item's information. Override
// this method to specialize. At this level, only accessibility properties are
// ported from the item to the cell. The headerFooter's class must match
// headerFooterClass for the given instance.
- (void)configureHeaderFooterView:(UITableViewHeaderFooterView*)headerFooter
                       withStyler:(ChromeTableViewStyler*)styler
    NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_HEADER_FOOTER_ITEM_H_
