// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/commerce/price_card/price_card_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_theme.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/grid_to_tab_transition_view.h"

@class GridCell;

// Informs the receiver of actions on the cell.
@protocol GridCellDelegate
- (void)closeButtonTappedForCell:(GridCell*)cell;
@end

// Values describing the editing state of the cell.
typedef NS_ENUM(NSUInteger, GridCellState) {
  GridCellStateNotEditing = 1,
  GridCellStateEditingUnselected,
  GridCellStateEditingSelected,
};

// A square-ish cell in a grid. Contains an icon, title, snapshot, and close
// button.
@interface GridCell : UICollectionViewCell
// Delegate to inform the grid of actions on the cell.
@property(nonatomic, weak) id<GridCellDelegate> delegate;
// The look of the cell.
@property(nonatomic, assign) GridTheme theme;
// Unique identifier for the cell's contents. This is used to ensure that
// updates in an asynchronous callback are only made if the item is the same.
@property(nonatomic, copy) NSString* itemIdentifier;
// Settable UI elements of the cell.
@property(nonatomic, weak) UIImage* icon;
@property(nonatomic, weak) UIImage* snapshot;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, assign) BOOL titleHidden;
@property(nonatomic, readonly) UIDragPreviewParameters* dragPreviewParameters;
// Sets to update and keep cell alpha in sync.
@property(nonatomic, assign) CGFloat opacity;
// The current state which the cell should display.
@property(nonatomic, assign) GridCellState state;
@property(nonatomic, weak) PriceCardView* priceCardView;

// Sets the price drop and displays the PriceViewCard.
- (void)setPriceDrop:(NSString*)price previousPrice:(NSString*)previousPrice;

// Fade in a new snapshot.
- (void)fadeInSnapshot:(UIImage*)snapshot;

// Starts the activity indicator animation.
- (void)showActivityIndicator;
// Stops the activity indicator animation.
- (void)hideActivityIndicator;
@end

// A GridCell for use in animated transitions that only shows selection state
// (that is, its content view is hidden).
@interface GridTransitionSelectionCell : GridCell
// Returns a transition selection cell with the same theme and frame as |cell|,
// but with no visible content view, no delegate, and no identifier.
+ (instancetype)transitionCellFromCell:(GridCell*)cell;
@end

@interface GridTransitionCell : GridCell <GridToTabTransitionView>
// Returns a cell with the same theme, icon, snapshot, title, and frame as
// |cell| (but no delegate or identifier) for use in animated transitions.
+ (instancetype)transitionCellFromCell:(GridCell*)cell;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_CELL_H_
