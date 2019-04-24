// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_BUTTON_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_BUTTON_ITEM_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// TableViewTextButtonItem contains the model for
// TableViewTextButtonCell.
@interface TableViewTextButtonItem : TableViewItem

// Text being displayed above the button.
@property(nonatomic, readwrite, strong) NSString* text;
// Text for cell button.
@property(nonatomic, readwrite, strong) NSString* buttonText;

// Button background color. Default is custom blue color.
@property(nonatomic, strong) UIColor* buttonBackgroundColor;

// Accessibility identifier that will assigned to the button.
@property(nonatomic, strong) NSString* buttonAccessibilityIdentifier;

@end

// TableViewTextButtonCell contains a textLabel and a UIbutton
// laid out vertically and centered.
@interface TableViewTextButtonCell : TableViewCell

// Cell text information.
@property(nonatomic, strong) UILabel* textLabel;
// Action button. Note: Set action method in the TableView datasource method.
@property(nonatomic, strong) UIButton* button;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_BUTTON_ITEM_H_
