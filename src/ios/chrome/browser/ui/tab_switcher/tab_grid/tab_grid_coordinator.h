// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/chrome_root_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_supporting.h"

@protocol ApplicationCommands;
class Browser;
@protocol BrowsingDataCommands;
@protocol TabGridCoordinatorDelegate;

@interface TabGridCoordinator : ChromeRootCoordinator

- (instancetype)initWithWindow:(UIWindow*)window
     applicationCommandEndpoint:
         (id<ApplicationCommands>)applicationCommandEndpoint
    browsingDataCommandEndpoint:
        (id<BrowsingDataCommands>)browsingDataCommandEndpoint
                 regularBrowser:(Browser*)regularBrowser
               incognitoBrowser:(Browser*)incognitoBrowser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithWindow:(UIWindow*)window NS_UNAVAILABLE;

@property(nonatomic, weak) id<TabGridCoordinatorDelegate> delegate;

// Updates the incognito browser. Should only be sets when both the current
// incognito browser and the new incognito browser are either nil or contain no
// tabs. This must be called after the incognito browser has been deleted
// because the incognito browser state is deleted.
@property(nonatomic, assign) Browser* incognitoBrowser;

// The view controller, if any, that is active.
@property(nonatomic, readonly, strong) UIViewController* activeViewController;

// If this property is YES, calls to |showTabSwitcher:completion:| and
// |showTabViewController:completion:| will present the given view controllers
// without animation.  This should only be used by unittests.
@property(nonatomic, readwrite, assign) BOOL animationsDisabledForTesting;

// Weak references to the regular and incognito browser view controllers,
// used to set up the thumb strip.
@property(nonatomic, weak) id<ThumbStripSupporting> regularThumbStripSupporting;
@property(nonatomic, weak) id<ThumbStripSupporting>
    incognitoThumbStripSupporting;

// If this property is YES, it means the tab grid is the main user interface at
// the moment. Returns NO if thumb strip is active.
@property(nonatomic, readonly, getter=isTabGridActive) BOOL tabGridActive;

// Stops all child coordinators then calls |completion|. |completion| is called
// whether or not child coordinators exist.
- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion;

// Perform any initial setup required for the appearance of the TabGrid.
- (void)prepareToShowTabGrid;

// Displays the TabGrid.
- (void)showTabGrid;

// Displays the given view controller. If |closeTabGrid| is yes, any
// TabSwitchers or other view controllers that may currently be visible will be
// replaced. Otherwise, the view controller is added to the current container.
// Runs the given |completion| block after the view controller is visible.
// |shouldCloseTabGrid| is only used for the thumb strip, where the
// tab container view controller is never dismissed.
- (void)showTabViewController:(UIViewController*)viewController
                    incognito:(BOOL)incognito
           shouldCloseTabGrid:(BOOL)shouldCloseTabGrid
                   completion:(ProceduralBlock)completion;

// Sets the |page| as the active (visible) one. The active page must not be the
// remote tabs.
- (void)setActivePage:(TabGridPage)page;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_COORDINATOR_H_
