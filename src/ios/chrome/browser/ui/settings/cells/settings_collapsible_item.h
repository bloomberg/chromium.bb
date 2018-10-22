// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_COLLAPSIBLE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_COLLAPSIBLE_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_cell.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"

// SettingsCollapsibleItem is the model class corresponding to
// SettingsCollapsibleCell.
@interface SettingsCollapsibleItem : CollectionViewTextItem

// YES if collapsed.
@property(nonatomic, assign, getter=isCollapsed) BOOL collapsed;

@end

// MDCCollectionViewCell that displays a cell with one string (on multiple
// lines), and chevron (pointing down if expanded or pointing up if collapsed).
@interface SettingsCollapsibleCell : CollectionViewTextCell

// YES if the chevron is pointing up.
@property(nonatomic, assign, getter=isCollapsed, readonly) BOOL collapsed;

// Moves the chevron to the position collapsed/expanded.
- (void)setCollapsed:(BOOL)collapsed animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_COLLAPSIBLE_ITEM_H_
