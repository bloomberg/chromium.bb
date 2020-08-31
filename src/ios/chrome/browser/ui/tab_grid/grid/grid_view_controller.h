// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_theme.h"

@protocol GridEmptyView;
@protocol GridImageDataSource;
@class GridTransitionLayout;
@class GridViewController;

// Protocol used to relay relevant user interactions from a grid UI.
@protocol GridViewControllerDelegate
// Tells the delegate that the item with |itemID| was selected in
// |gridViewController|.
- (void)gridViewController:(GridViewController*)gridViewController
       didSelectItemWithID:(NSString*)itemID;
// Tells the delegate that the item with |itemID| was closed in
// |gridViewController|.
- (void)gridViewController:(GridViewController*)gridViewController
        didCloseItemWithID:(NSString*)itemID;
// Tells the delegate that the item at |sourceIndex| was moved to
// |destinationIndex|.
- (void)gridViewController:(GridViewController*)gridViewController
         didMoveItemWithID:(NSString*)itemID
                   toIndex:(NSUInteger)destinationIndex;
// Tells the delegate that the the number of items in |gridViewController|
// changed to |count|.
- (void)gridViewController:(GridViewController*)gridViewController
        didChangeItemCount:(NSUInteger)count;
@end

// A view controller that contains a grid of items.
@interface GridViewController : UIViewController<GridConsumer>
// The gridView is accessible to manage the content inset behavior.
@property(nonatomic, readonly) UIScrollView* gridView;
// The view that is shown when there are no items.
@property(nonatomic, strong) UIView<GridEmptyView>* emptyStateView;
// Returns YES if the grid has no items.
@property(nonatomic, readonly, getter=isGridEmpty) BOOL gridEmpty;
// The visual look of the grid.
@property(nonatomic, assign) GridTheme theme;
// Delegate is informed of user interactions in the grid UI.
@property(nonatomic, weak) id<GridViewControllerDelegate> delegate;
// Data source for images.
@property(nonatomic, weak) id<GridImageDataSource> imageDataSource;
// YES if the selected cell is visible in the grid.
@property(nonatomic, readonly, getter=isSelectedCellVisible)
    BOOL selectedCellVisible;
// YES if the gid should show cell selection updates. This would be set to NO,
// for example, if the grid was about to be transitioned out of.
@property(nonatomic, assign) BOOL showsSelectionUpdates;

// Returns the layout of the grid for use in an animated transition.
- (GridTransitionLayout*)transitionLayout;

// Notifies the ViewController that its content is being displayed or hidden.
- (void)contentWillAppearAnimated:(BOOL)animated;
- (void)contentWillDisappear;

// Notifies the grid that it is about to be dismissed.
- (void)prepareForDismissal;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_H_
