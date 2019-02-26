// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/popup_menu/popup_menu_consumer.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol PopupMenuItem;
@protocol PopupMenuTableViewControllerCommands;
@protocol LoadQueryCommands;

// TableViewController for the popup menu.
@interface PopupMenuTableViewController
    : ChromeTableViewController<PopupMenuConsumer>

// The model of this controller.
@property(nonatomic, readonly, strong)
    TableViewModel<TableViewItem<PopupMenuItem>*>* tableViewModel;
// Dispatcher.
@property(nonatomic, weak)
    id<ApplicationCommands, BrowserCommands, LoadQueryCommands>
        dispatcher;
// Presenting ViewController for the ViewController needing to be presented as
// result of an interaction with the popup.
@property(nonatomic, weak) UIViewController* baseViewController;

// Initializers.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

// Selects the row at |point|. This is the same as tapping the row. Point must
// be in window base coordinates.
- (void)selectRowAtPoint:(CGPoint)point;
// Focuses the row at |point|. This adds an highlight to the row. Point must be
// in window base coordinates.
- (void)focusRowAtPoint:(CGPoint)point;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_TABLE_VIEW_CONTROLLER_H_
