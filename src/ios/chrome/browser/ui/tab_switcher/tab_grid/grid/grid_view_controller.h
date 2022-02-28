// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/gestures/layout_switcher.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_theme.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_supporting.h"

@protocol GridContextMenuProvider;
@protocol GridDragDropHandler;
@protocol GridEmptyView;
@protocol GridImageDataSource;
@protocol GridShareableItemsProvider;
@class GridTransitionLayout;
@class GridViewController;
@protocol IncognitoReauthCommands;
@protocol PriceCardDataSource;
@protocol ThumbStripCommands;

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
// Tells the delegate that the plus sign was tapped in |gridViewController|,
// i.e., there was an intention to create a new item.
- (void)didTapPlusSignInGridViewController:
    (GridViewController*)gridViewController;
// Tells the delegate that the item at |sourceIndex| was moved to
// |destinationIndex|.
- (void)gridViewController:(GridViewController*)gridViewController
         didMoveItemWithID:(NSString*)itemID
                   toIndex:(NSUInteger)destinationIndex;
// Tells the delegate that the the number of items in |gridViewController|
// changed to |count|.
- (void)gridViewController:(GridViewController*)gridViewController
        didChangeItemCount:(NSUInteger)count;

// Tells the delegate that the visibility of the last item of the grid changed.
- (void)didChangeLastItemVisibilityInGridViewController:
    (GridViewController*)gridViewController;

// Tells the delegate when the currently displayed content is hidden from the
// user until they authenticate. Used for incognito biometric authentication.
- (void)gridViewController:(GridViewController*)gridViewController
    contentNeedsAuthenticationChanged:(BOOL)needsAuth;

// Tells the delegate that the grid view controller's scroll view will begin
// dragging.
- (void)gridViewControllerWillBeginDragging:
    (GridViewController*)gridViewController;

// Tells the delegate that the grid view controller cells will begin dragging.
- (void)gridViewControllerDragSessionWillBegin:
    (GridViewController*)gridViewController;
// Tells the delegate that the grid view controller cells did end dragging.
- (void)gridViewControllerDragSessionDidEnd:
    (GridViewController*)gridViewController;

@end

// A view controller that contains a grid of items.
@interface GridViewController : UIViewController <GridConsumer,
                                                  LayoutSwitcher,
                                                  IncognitoReauthConsumer,
                                                  ThumbStripSupporting>
// The gridView is accessible to manage the content inset behavior.
@property(nonatomic, readonly) UIScrollView* gridView;
// The view that is shown when there are no items.
@property(nonatomic, strong) UIView<GridEmptyView>* emptyStateView;
// Returns YES if the grid has no items.
@property(nonatomic, readonly, getter=isGridEmpty) BOOL gridEmpty;
// The visual look of the grid.
@property(nonatomic, assign) GridTheme theme;
// The current mode (normal, selection) for the grid.
@property(nonatomic, assign) TabGridMode mode;
// Handler for reauth commands.
@property(nonatomic, weak) id<IncognitoReauthCommands> reauthHandler;
// Handler for thumbstrip commands.
@property(nonatomic, weak) id<ThumbStripCommands> thumbStripHandler;
// Delegate is informed of user interactions in the grid UI.
@property(nonatomic, weak) id<GridViewControllerDelegate> delegate;
// Handles drag and drop interactions that involved the model layer.
@property(nonatomic, weak) id<GridDragDropHandler> dragDropHandler;
// Data source for images.
@property(nonatomic, weak) id<GridImageDataSource> imageDataSource;
// Data source for acquiring data to power PriceCardView
@property(nonatomic, weak) id<PriceCardDataSource> priceCardDataSource;
// YES if the selected cell is visible in the grid.
@property(nonatomic, readonly, getter=isSelectedCellVisible)
    BOOL selectedCellVisible;
// YES if the gid should show cell selection updates. This would be set to NO,
// for example, if the grid was about to be transitioned out of.
@property(nonatomic, assign) BOOL showsSelectionUpdates;
// The fraction of the last item of the grid that is visible.
@property(nonatomic, assign, readonly) CGFloat fractionVisibleOfLastItem;
// YES when the current contents are hidden from the user before a successful
// biometric authentication.
@property(nonatomic, assign) BOOL contentNeedsAuthentication;
// Provider of context menu configurations for the tabs in the grid.
@property(nonatomic, weak) id<GridContextMenuProvider> menuProvider;
// Provider of shareable state for tabs in the grid.
@property(nonatomic, weak) id<GridShareableItemsProvider>
    shareableItemsProvider;

// The item IDs of selected items for editing.
@property(nonatomic, readonly) NSArray<NSString*>* selectedItemIDsForEditing;
// The item IDs of selected items for editing which are shareable outside of the
// application.
@property(nonatomic, readonly)
    NSArray<NSString*>* selectedShareableItemIDsForEditing;

// Whether or not all items are selected. NO if |mode| is not
// TabGridModeSelection.
@property(nonatomic, readonly) BOOL allItemsSelectedForEditing;

// Opacity of grid cells that are not the selected tab.
@property(nonatomic, assign) CGFloat notSelectedTabCellOpacity;

// Returns the layout of the grid for use in an animated transition.
- (GridTransitionLayout*)transitionLayout;

// Notifies the ViewController that its content is being displayed or hidden.
- (void)contentWillAppearAnimated:(BOOL)animated;
- (void)contentWillDisappear;

// Notifies the grid that it is about to be dismissed.
- (void)prepareForDismissal;

// Selects all items in the grid for editing. No-op if |mode| is not
// TabGridModeSelection.
- (void)selectAllItemsForEditing;

// Deselects all items in the grid for editing. No-op if |mode| is not
// TabGridModeSelection.
- (void)deselectAllItemsForEditing;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_VIEW_CONTROLLER_H_
