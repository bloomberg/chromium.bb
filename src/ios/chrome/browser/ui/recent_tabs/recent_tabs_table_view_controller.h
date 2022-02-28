// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_consumer.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"
#include "ui/base/window_open_disposition.h"

class Browser;
enum class UrlLoadStrategy;

namespace synced_sessions {
class DistantSession;
}

@protocol ApplicationCommands;
@protocol RecentTabsMenuProvider;
@protocol RecentTabsTableViewControllerDelegate;
@protocol RecentTabsPresentationDelegate;
@protocol TableViewFaviconDataSource;

@interface RecentTabsTableViewController
    : ChromeTableViewController <RecentTabsConsumer,
                                 UIAdaptivePresentationControllerDelegate>
// The Browser for the tabs being restored. It's an error to pass a nullptr
// Browser.
@property(nonatomic, assign) Browser* browser;
// The command handler used by this ViewController.
@property(nonatomic, weak) id<ApplicationCommands> handler;
// Opaque instructions on how to open urls.
@property(nonatomic) UrlLoadStrategy loadStrategy;
// Disposition for tabs restored by this object. Defaults to CURRENT_TAB.
@property(nonatomic, assign) WindowOpenDisposition restoredTabDisposition;
// RecentTabsTableViewControllerDelegate delegate.
@property(nonatomic, weak) id<RecentTabsTableViewControllerDelegate> delegate;
// Whether the updates of the RecentTabs should be ignored. Setting this to NO
// would trigger a reload of the TableView.
@property(nonatomic, assign) BOOL preventUpdates;

// Delegate to present the tab UI.
@property(nonatomic, weak) id<RecentTabsPresentationDelegate>
    presentationDelegate;

// Data source for images.
@property(nonatomic, weak) id<TableViewFaviconDataSource> imageDataSource;

// Provider of menu configurations for the recentTabs component.
@property(nonatomic, weak) id<RecentTabsMenuProvider> menuProvider;

// Multi-window session for this vc's recent tabs.
@property(nonatomic, assign) UISceneSession* session;

// Initializers.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Returns Sessions corresponding to the given |sectionIdentifier|.
- (synced_sessions::DistantSession const*)sessionForTableSectionWithIdentifier:
    (NSInteger)sectionIdentifer;

// Hides Sessions corresponding to the given the table view's
// |sectionIdentifier|.
- (void)removeSessionAtTableSectionWithIdentifier:(NSInteger)sectionIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_RECENT_TABS_TABLE_VIEW_CONTROLLER_H_
