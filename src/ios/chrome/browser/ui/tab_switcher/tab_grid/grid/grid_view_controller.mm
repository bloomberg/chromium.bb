// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller.h"

#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#import "base/numerics/safe_conversions.h"
#include "ios/chrome/browser/procedural_block_types.h"
#import "ios/chrome/browser/ui/commands/thumb_strip_commands.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_commands.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_context_menu_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_drag_drop_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_empty_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_image_data_source.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_shareable_items_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/horizontal_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/plus_sign_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/grid_transition_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/public/provider/chrome/browser/modals/modals_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Constants used in the spring animation when inserting items in the thumb
// strip.
constexpr CGFloat kSpringAnimationDuration = 0.4;
constexpr CGFloat kSpringAnimationDamping = 0.6;
constexpr CGFloat kSpringAnimationInitialVelocity = 1.0;

NSString* const kCellIdentifier = @"GridCellIdentifier";

NSString* const kPlusSignCellIdentifier = @"PlusSignCellIdentifier";

// Creates an NSIndexPath with |index| in section 0.
NSIndexPath* CreateIndexPath(NSInteger index) {
  return [NSIndexPath indexPathForItem:index inSection:0];
}

}  // namespace

@interface GridViewController () <GridCellDelegate,
                                  UICollectionViewDataSource,
                                  UICollectionViewDelegate,
                                  UICollectionViewDragDelegate,
                                  UICollectionViewDropDelegate,
                                  UIPointerInteractionDelegate>
// A collection view of items in a grid format.
@property(nonatomic, weak) UICollectionView* collectionView;
// A view to obscure incognito content when the user isn't authorized to
// see it.
@property(nonatomic, strong) IncognitoReauthView* blockingView;
// The local model backing the collection view.
@property(nonatomic, strong) NSMutableArray<TabSwitcherItem*>* items;
// Identifier of the selected item. This value is disregarded if |self.items| is
// empty. This bookkeeping is done to set the correct selection on
// |-viewWillAppear:|.
@property(nonatomic, copy) NSString* selectedItemID;
// Index of the selected item in |items|.
@property(nonatomic, readonly) NSUInteger selectedIndex;
// Items selected for editing.
@property(nonatomic, strong) NSMutableSet<NSString*>* selectedEditingItemIDs;
// Items selected for editing which are shareable outside of the app.
@property(nonatomic, strong)
    NSMutableSet<NSString*>* selectedSharableEditingItemIDs;
// ID of the last item to be inserted. This is used to track if the active tab
// was newly created when building the animation layout for transitions.
@property(nonatomic, copy) NSString* lastInsertedItemID;
// Animator to show or hide the empty state.
@property(nonatomic, strong) UIViewPropertyAnimator* emptyStateAnimator;
// The current layout for the tab switcher.
@property(nonatomic, strong) FlowLayout* currentLayout;
// The layout for the tab grid.
@property(nonatomic, strong) GridLayout* gridLayout;
// The layout for the thumb strip.
@property(nonatomic, strong) HorizontalLayout* horizontalLayout;
// By how much the user scrolled past the view's content size. A negative value
// means the user hasn't scrolled past the end of the scroll view.
@property(nonatomic, assign, readonly) CGFloat offsetPastEndOfScrollView;
// Cells for which pointer interactions have been added. Pointer interactions
// should only be added to displayed cells (not transition cells). This is only
// expected to get as large as the number of reusable cells in memory.
@property(nonatomic, strong)
    NSHashTable<UICollectionViewCell*>* pointerInteractionCells API_AVAILABLE(
        ios(13.4));
// The transition layout either from grid to horizontal layout or from
// horizontal to grid layout.
@property(nonatomic, strong)
    UICollectionViewTransitionLayout* gridHorizontalTransitionLayout;

// Gesture recognizer to dismiss the thumb strip.
@property(nonatomic, strong)
    UITapGestureRecognizer* thumbStripDismissRecognizer;

// YES while batch updates and the batch update completion are being performed.
@property(nonatomic) BOOL updating;

@end

@implementation GridViewController

@synthesize thumbStripEnabled = _thumbStripEnabled;

- (instancetype)init {
  if (self = [super init]) {
    _items = [[NSMutableArray<TabSwitcherItem*> alloc] init];
    _selectedEditingItemIDs = [[NSMutableSet<NSString*> alloc] init];
    _selectedSharableEditingItemIDs = [[NSMutableSet<NSString*> alloc] init];
    _showsSelectionUpdates = YES;
    _mode = TabGridModeNormal;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  self.horizontalLayout = [[HorizontalLayout alloc] init];
  self.gridLayout = [[GridLayout alloc] init];
  self.currentLayout = self.gridLayout;

  UICollectionView* collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:self.currentLayout];
  [collectionView registerClass:[GridCell class]
      forCellWithReuseIdentifier:kCellIdentifier];
  [collectionView registerClass:[PlusSignCell class]
      forCellWithReuseIdentifier:kPlusSignCellIdentifier];
  // If this stays as the default |YES|, then cells aren't highlighted
  // immediately on touch, but after a short delay.
  collectionView.delaysContentTouches = NO;
  collectionView.dataSource = self;
  collectionView.delegate = self;
  collectionView.backgroundView = [[UIView alloc] init];
  collectionView.backgroundView.backgroundColor =
      [UIColor colorNamed:kGridBackgroundColor];
  collectionView.backgroundView.accessibilityIdentifier =
      kGridBackgroundIdentifier;

  // CollectionView, in contrast to TableView, doesn’t inset the
  // cell content to the safe area guide by default. We will just manage the
  // collectionView contentInset manually to fit in the safe area instead.
  collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;
  self.collectionView = collectionView;
  self.view = collectionView;

  // A single selection collection view's default behavior is to momentarily
  // deselect the selected cell on touch down then select the new cell on touch
  // up. In this tab grid, the selection ring should stay visible on the
  // selected cell on touch down. Multiple selection disables the deselection
  // behavior. Multiple selection will not actually be possible since
  // |-collectionView:shouldSelectItemAtIndexPath:| returns NO.
  collectionView.allowsMultipleSelection = YES;
  collectionView.dragDelegate = self;
  collectionView.dropDelegate = self;
  collectionView.dragInteractionEnabled = YES;

  if (@available(iOS 13.4, *)) {
      self.pointerInteractionCells =
          [NSHashTable<UICollectionViewCell*> weakObjectsHashTable];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self contentWillAppearAnimated:animated];
}

- (void)viewWillDisappear:(BOOL)animated {
  [self contentWillDisappear];
  [super viewWillDisappear:animated];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateFractionVisibleOfLastItem];
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self.collectionView.collectionViewLayout invalidateLayout];
}

#pragma mark - Public

- (UIScrollView*)gridView {
  return self.collectionView;
}

- (void)setEmptyStateView:(UIView<GridEmptyView>*)emptyStateView {
  if (_emptyStateView)
    [_emptyStateView removeFromSuperview];
  _emptyStateView = emptyStateView;
  emptyStateView.scrollViewContentInsets =
      self.collectionView.adjustedContentInset;
  emptyStateView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.collectionView.backgroundView addSubview:emptyStateView];
  id<LayoutGuideProvider> safeAreaGuide =
      self.collectionView.backgroundView.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [self.collectionView.backgroundView.centerYAnchor
        constraintEqualToAnchor:emptyStateView.centerYAnchor],
    [safeAreaGuide.leadingAnchor
        constraintEqualToAnchor:emptyStateView.leadingAnchor],
    [safeAreaGuide.trailingAnchor
        constraintEqualToAnchor:emptyStateView.trailingAnchor],
    [emptyStateView.topAnchor
        constraintGreaterThanOrEqualToAnchor:safeAreaGuide.topAnchor],
    [emptyStateView.bottomAnchor
        constraintLessThanOrEqualToAnchor:safeAreaGuide.bottomAnchor],
  ]];
}

- (BOOL)isGridEmpty {
  return self.items.count == 0;
}

- (void)setMode:(TabGridMode)mode {
  _mode = mode;

  // Reloading specific sections in a |performBatchUpdates| fades the changes in
  // rather than reloads the collection view with a harsh flash.
  __weak GridViewController* weakSelf = self;
  [self.collectionView
      performBatchUpdates:^{
        GridViewController* strongSelf = weakSelf;
        if (!strongSelf || !strongSelf.collectionView) {
          return;
        }

        NSRange allSectionsRange = NSMakeRange(
            /*location=*/0, strongSelf.collectionView.numberOfSections);
        NSIndexSet* allSectionsIndexSet =
            [NSIndexSet indexSetWithIndexesInRange:allSectionsRange];
        [strongSelf.collectionView reloadSections:allSectionsIndexSet];
      }
               completion:nil];

  // Clear items when exiting selection mode.
  if (mode == TabGridModeNormal) {
    [self.selectedEditingItemIDs removeAllObjects];
    [self.selectedSharableEditingItemIDs removeAllObjects];
    // After transition from the selection mode to the normal mode, the
    // selection border doesn't show around the selection item. The collection
    // view needs to be updated with the selected item again for it to appear
    // correctly.
    [self.collectionView
        selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                     animated:NO
               scrollPosition:UICollectionViewScrollPositionNone];
  }
}

- (BOOL)isSelectedCellVisible {
  // The collection view's selected item may not have updated yet, so use the
  // selected index.
  NSUInteger selectedIndex = self.selectedIndex;
  if (selectedIndex == NSNotFound)
    return NO;
  NSIndexPath* selectedIndexPath = CreateIndexPath(selectedIndex);
  return [self.collectionView.indexPathsForVisibleItems
      containsObject:selectedIndexPath];
}

- (GridTransitionLayout*)transitionLayout {
  [self.collectionView layoutIfNeeded];
  NSMutableArray<GridTransitionItem*>* items = [[NSMutableArray alloc] init];
  GridTransitionActiveItem* activeItem;
  GridTransitionItem* selectionItem;
  for (NSIndexPath* path in self.collectionView.indexPathsForVisibleItems) {
    GridCell* cell = base::mac::ObjCCastStrict<GridCell>(
        [self.collectionView cellForItemAtIndexPath:path]);
    UICollectionViewLayoutAttributes* attributes =
        [self.collectionView layoutAttributesForItemAtIndexPath:path];
    // Normalize frame to window coordinates. The attributes class applies this
    // change to the other properties such as center, bounds, etc.
    attributes.frame = [self.collectionView convertRect:attributes.frame
                                                 toView:nil];
    if ([cell.itemIdentifier isEqualToString:self.selectedItemID]) {
      GridTransitionCell* activeCell =
          [GridTransitionCell transitionCellFromCell:cell];
      activeItem = [GridTransitionActiveItem itemWithCell:activeCell
                                                   center:attributes.center
                                                     size:attributes.size];
      // If the active item is the last inserted item, it needs to be animated
      // differently.
      if ([cell.itemIdentifier isEqualToString:self.lastInsertedItemID])
        activeItem.isAppearing = YES;
      selectionItem = [GridTransitionItem
          itemWithCell:[GridTransitionSelectionCell transitionCellFromCell:cell]
                center:attributes.center];
    } else {
      UIView* cellSnapshot = [cell snapshotViewAfterScreenUpdates:YES];
      GridTransitionItem* item =
          [GridTransitionItem itemWithCell:cellSnapshot
                                    center:attributes.center];
      [items addObject:item];
    }
  }
  return [GridTransitionLayout layoutWithInactiveItems:items
                                            activeItem:activeItem
                                         selectionItem:selectionItem];
}

- (void)prepareForDismissal {
  // Stop animating the collection view to prevent the insertion animation from
  // interfering with the tab presentation animation.
  self.currentLayout.animatesItemUpdates = NO;
}

- (void)contentWillAppearAnimated:(BOOL)animated {
  self.currentLayout.animatesItemUpdates = YES;
  [self.collectionView reloadData];
  // Selection is invalid if there are no items.
  if (self.items.count == 0) {
    [self animateEmptyStateIn];
    return;
  }
  UICollectionViewScrollPosition scrollPosition =
      (self.currentLayout == self.horizontalLayout)
          ? UICollectionViewScrollPositionCenteredHorizontally
          : UICollectionViewScrollPositionTop;
  [self.collectionView selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                                    animated:NO
                              scrollPosition:scrollPosition];
  // Update the delegate, in case it wasn't set when |items| was populated.
  [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  [self removeEmptyStateAnimated:NO];
  self.lastInsertedItemID = nil;
}

- (void)contentWillDisappear {
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  if (self.thumbStripEnabled) {
    // The PlusSignCell (new item button) is always appended at the end of the
    // collection.
    return base::checked_cast<NSInteger>(self.items.count + 1);
  }
  return base::checked_cast<NSInteger>(self.items.count);
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  NSUInteger itemIndex = base::checked_cast<NSUInteger>(indexPath.item);
  UICollectionViewCell* cell;

  if ([self isIndexPathForPlusSignCell:indexPath]) {
    cell = [collectionView
        dequeueReusableCellWithReuseIdentifier:kPlusSignCellIdentifier
                                  forIndexPath:indexPath];
    PlusSignCell* plusSignCell = base::mac::ObjCCastStrict<PlusSignCell>(cell);
    plusSignCell.theme = self.theme;
  } else {
    // In some cases this is called with an indexPath.item that's beyond (by 1)
    // the bounds of self.items -- see crbug.com/1068136. Presumably this is a
    // race condition where an item has been deleted at the same time as the
    // collection is doing layout (potentially during rotation?). DCHECK to
    // catch this in debug, and then in production fudge by duplicating the last
    // cell. The assumption is that there will be another, correct layout
    // shortly after the incorrect one.
    DCHECK_LT(itemIndex, self.items.count);
    // Outside of debug builds, keep array bounds valid.
    if (itemIndex >= self.items.count)
      itemIndex = self.items.count - 1;

    TabSwitcherItem* item = self.items[itemIndex];
    cell =
        [collectionView dequeueReusableCellWithReuseIdentifier:kCellIdentifier
                                                  forIndexPath:indexPath];
    cell.accessibilityIdentifier = [NSString
        stringWithFormat:@"%@%ld", kGridCellIdentifierPrefix, itemIndex];
    GridCell* gridCell = base::mac::ObjCCastStrict<GridCell>(cell);
    [self configureCell:gridCell withItem:item];
  }
  // Set the z index of cells so that lower rows are moving behind the upper
  // rows during transitions between grid and horizontal layouts.
  cell.layer.zPosition = self.items.count - itemIndex;

  if (@available(iOS 13.4, *)) {
      if (![self.pointerInteractionCells containsObject:cell]) {
        [cell addInteraction:[[UIPointerInteraction alloc]
                                 initWithDelegate:self]];
        // |self.pointerInteractionCells| is only expected to get as large as
        // the number of reusable cells in memory.
        [self.pointerInteractionCells addObject:cell];
      }
  }
  return cell;
}

#pragma mark - UICollectionViewDelegate

// This prevents the user from dragging a cell past the plus sign cell (the last
// cell in the collection view).
- (NSIndexPath*)collectionView:(UICollectionView*)collectionView
    targetIndexPathForMoveFromItemAtIndexPath:(NSIndexPath*)originalIndexPath
                          toProposedIndexPath:(NSIndexPath*)proposedIndexPath {
  if ([self isIndexPathForPlusSignCell:proposedIndexPath]) {
    return CreateIndexPath(proposedIndexPath.item - 1);
  }
  return proposedIndexPath;
}

// This method is used instead of -didSelectItemAtIndexPath, because any
// selection events will be signalled through the model layer and handled in
// the GridConsumer -selectItemWithID: method.
- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [self tappedItemAtIndexPath:indexPath];
  // Tapping on a non-selected cell should not select it immediately. The
  // delegate will trigger a transition to show the item.
  return NO;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldDeselectItemAtIndexPath:(NSIndexPath*)indexPath {
  [self tappedItemAtIndexPath:indexPath];
  // Tapping on the current selected cell should not deselect it.
  return NO;
}

- (UIContextMenuConfiguration*)collectionView:(UICollectionView*)collectionView
    contextMenuConfigurationForItemAtIndexPath:(NSIndexPath*)indexPath
                                         point:(CGPoint)point {
  // Context menu shouldn't appear in the selection mode.
  if (!IsTabGridContextMenuEnabled() || _mode == TabGridModeSelection) {
    return nil;
  }
  GridCell* cell = base::mac::ObjCCastStrict<GridCell>(
      [self.collectionView cellForItemAtIndexPath:indexPath]);
  return [self.menuProvider contextMenuConfigurationForGridCell:cell];
}

#pragma mark - UIPointerInteractionDelegate

- (UIPointerRegion*)pointerInteraction:(UIPointerInteraction*)interaction
                      regionForRequest:(UIPointerRegionRequest*)request
                         defaultRegion:(UIPointerRegion*)defaultRegion
    API_AVAILABLE(ios(13.4)) {
  return defaultRegion;
}

- (UIPointerStyle*)pointerInteraction:(UIPointerInteraction*)interaction
                       styleForRegion:(UIPointerRegion*)region
    API_AVAILABLE(ios(13.4)) {
  UIPointerLiftEffect* effect = [UIPointerLiftEffect
      effectWithPreview:[[UITargetedPreview alloc]
                            initWithView:interaction.view]];
  return [UIPointerStyle styleWithEffect:effect shape:nil];
}

#pragma mark - UICollectionViewDragDelegate

- (void)collectionView:(UICollectionView*)collectionView
    dragSessionWillBegin:(id<UIDragSession>)session {
  [self.delegate gridViewControllerDragSessionWillBegin:self];
}

- (void)collectionView:(UICollectionView*)collectionView
     dragSessionDidEnd:(id<UIDragSession>)session {
  [self.delegate gridViewControllerDragSessionDidEnd:self];
}

- (NSArray<UIDragItem*>*)collectionView:(UICollectionView*)collectionView
           itemsForBeginningDragSession:(id<UIDragSession>)session
                            atIndexPath:(NSIndexPath*)indexPath {
  if ([self isIndexPathForPlusSignCell:indexPath]) {
    // Return an empty array because the plus sign cell should not be dragged.
    return @[];
  }
  if (_mode != TabGridModeSelection) {
    TabSwitcherItem* item = self.items[indexPath.item];
    return @[ [self.dragDropHandler dragItemForItemWithID:item.identifier] ];
  }

  // Make sure that the long pressed cell is selected before initiating a drag
  // from it.
  NSUInteger index = base::checked_cast<NSUInteger>(indexPath.item);
  NSString* itemID = self.items[index].identifier;
  if (![self isItemWithIDSelectedForEditing:itemID]) {
    [self tappedItemAtIndexPath:indexPath];
  }

  NSMutableArray<UIDragItem*>* dragItems = [[NSMutableArray alloc] init];
  for (NSString* itemID in self.selectedEditingItemIDs) {
    [dragItems addObject:[self.dragDropHandler dragItemForItemWithID:itemID]];
  }
  return dragItems;
}

- (NSArray<UIDragItem*>*)collectionView:(UICollectionView*)collectionView
            itemsForAddingToDragSession:(id<UIDragSession>)session
                            atIndexPath:(NSIndexPath*)indexPath
                                  point:(CGPoint)point {
  // TODO(crbug.com/1087848): Allow multi-select.
  // Prevent more items from getting added to the drag session.
  return @[];
}

- (UIDragPreviewParameters*)collectionView:(UICollectionView*)collectionView
    dragPreviewParametersForItemAtIndexPath:(NSIndexPath*)indexPath {
  if ([self isIndexPathForPlusSignCell:indexPath]) {
    // Return nil so that the plus sign cell doesn't superpose the dragged cell.
    return nil;
  }
  GridCell* gridCell = base::mac::ObjCCastStrict<GridCell>(
      [self.collectionView cellForItemAtIndexPath:indexPath]);
  return gridCell.dragPreviewParameters;
}

#pragma mark - UICollectionViewDropDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    canHandleDropSession:(id<UIDropSession>)session {
  return YES;
}

- (UICollectionViewDropProposal*)
              collectionView:(UICollectionView*)collectionView
        dropSessionDidUpdate:(id<UIDropSession>)session
    withDestinationIndexPath:(NSIndexPath*)destinationIndexPath {
  // This is how the explicit forbidden icon or (+) copy icon is shown. Move has
  // no explicit icon.
  UIDropOperation dropOperation =
      [self.dragDropHandler dropOperationForDropSession:session];
  return [[UICollectionViewDropProposal alloc]
      initWithDropOperation:dropOperation
                     intent:
                         UICollectionViewDropIntentInsertAtDestinationIndexPath];
}

- (void)collectionView:(UICollectionView*)collectionView
    performDropWithCoordinator:
        (id<UICollectionViewDropCoordinator>)coordinator {
  NSArray<id<UICollectionViewDropItem>>* items = coordinator.items;

  for (id<UICollectionViewDropItem> item in items) {
    // Append to the end of the collection, unless drop index is specified.
    // The sourceIndexPath is nil if the drop item is not from the same
    // collection view. Set the destinationIndex to reflect the addition of an
    // item.
    NSUInteger destinationIndex =
        item.sourceIndexPath ? self.items.count - 1 : self.items.count;
    if (coordinator.destinationIndexPath) {
      destinationIndex =
          base::checked_cast<NSUInteger>(coordinator.destinationIndexPath.item);
    }
    if (self.thumbStripEnabled) {
      // The sourceIndexPath is nil if the drop item is not from the same
      // collection view.
      NSUInteger plusSignCellIndex =
          item.sourceIndexPath ? self.items.count : self.items.count + 1;
      // Can't use [self isIndexPathForPlusSignCell:] here because the index of
      // the plus sign cell in this point in code depends on
      // |item.sourceIndexPath|.
      // I.e., in this point in code, |collectionView.numberOfItemsInSection| is
      // equal to |self.items.count + 1|.
      if (destinationIndex == plusSignCellIndex) {
        // Prevent the cell from being dropped where the plus sign cell is.
        destinationIndex = plusSignCellIndex - 1;
      }
    }
    NSIndexPath* dropIndexPath = CreateIndexPath(destinationIndex);
    // Drop synchronously if local object is available.
    if (item.dragItem.localObject) {
      [coordinator dropItem:item.dragItem toItemAtIndexPath:dropIndexPath];
      // The sourceIndexPath is non-nil if the drop item is from this same
      // collection view.
      [self.dragDropHandler dropItem:item.dragItem
                             toIndex:destinationIndex
                  fromSameCollection:(item.sourceIndexPath != nil)];
    } else {
      // Drop asynchronously if local object is not available.
      UICollectionViewDropPlaceholder* placeholder =
          [[UICollectionViewDropPlaceholder alloc]
              initWithInsertionIndexPath:dropIndexPath
                         reuseIdentifier:kCellIdentifier];
      placeholder.cellUpdateHandler = ^(UICollectionViewCell* placeholderCell) {
        GridCell* gridCell =
            base::mac::ObjCCastStrict<GridCell>(placeholderCell);
        gridCell.theme = self.theme;
      };
      placeholder.previewParametersProvider =
          ^UIDragPreviewParameters*(UICollectionViewCell* placeholderCell) {
        GridCell* gridCell =
            base::mac::ObjCCastStrict<GridCell>(placeholderCell);
        return gridCell.dragPreviewParameters;
      };

      id<UICollectionViewDropPlaceholderContext> context =
          [coordinator dropItem:item.dragItem toPlaceholder:placeholder];
      [self.dragDropHandler dropItemFromProvider:item.dragItem.itemProvider
                                         toIndex:destinationIndex
                              placeholderContext:context];
    }
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidChangeAdjustedContentInset:(UIScrollView*)scrollView {
  self.emptyStateView.scrollViewContentInsets = scrollView.contentInset;
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  [self.delegate gridViewControllerWillBeginDragging:self];
}

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  if (!self.thumbStripEnabled)
    return;
  [self updateFractionVisibleOfLastItem];
}

#pragma mark - GridCellDelegate

- (void)closeButtonTappedForCell:(GridCell*)cell {
  [self.delegate gridViewController:self
                 didCloseItemWithID:cell.itemIdentifier];
  // Record when a tab is closed via the X.
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCloseControlTapped"));
}

#pragma mark - IncognitoReauthConsumer

- (void)setItemsRequireAuthentication:(BOOL)require {
  self.contentNeedsAuthentication = require;
  [self.delegate gridViewController:self
      contentNeedsAuthenticationChanged:require];

  if (require) {
    if (!self.blockingView) {
      self.blockingView = [[IncognitoReauthView alloc] init];
      self.blockingView.translatesAutoresizingMaskIntoConstraints = NO;
      self.blockingView.layer.zPosition = FLT_MAX;
      // No need to show tab switcher button when already in the tab switcher.
      self.blockingView.tabSwitcherButton.hidden = YES;
      // Hide the logo.
      self.blockingView.logoView.hidden = YES;

      [self.blockingView.authenticateButton
                 addTarget:self.reauthHandler
                    action:@selector(authenticateIncognitoContent)
          forControlEvents:UIControlEventTouchUpInside];
    }

    [self.view addSubview:self.blockingView];
    self.blockingView.alpha = 1;
    AddSameConstraints(self.collectionView.frameLayoutGuide, self.blockingView);
  } else {
    [UIView animateWithDuration:0.2
        animations:^{
          self.blockingView.alpha = 0;
        }
        completion:^(BOOL finished) {
          if (self.contentNeedsAuthentication) {
            self.blockingView.alpha = 1;
          } else {
            [self.blockingView removeFromSuperview];
          }
        }];
  }
}

#pragma mark - GridConsumer

- (void)populateItems:(NSArray<TabSwitcherItem*>*)items
       selectedItemID:(NSString*)selectedItemID {
#ifndef NDEBUG
  // Consistency check: ensure no IDs are duplicated.
  NSMutableSet<NSString*>* identifiers = [[NSMutableSet alloc] init];
  for (TabSwitcherItem* item in items) {
    [identifiers addObject:item.identifier];
  }
  CHECK_EQ(identifiers.count, items.count);
#endif

  self.items = [items mutableCopy];
  self.selectedItemID = selectedItemID;
  [self.selectedEditingItemIDs removeAllObjects];
  [self.selectedSharableEditingItemIDs removeAllObjects];
  [self.collectionView reloadData];
  [self.collectionView selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                                    animated:YES
                              scrollPosition:UICollectionViewScrollPositionTop];
  if (self.items.count > 0) {
    [self removeEmptyStateAnimated:YES];
  } else {
    [self animateEmptyStateIn];
  }
  // Whether the view is visible or not, the delegate must be updated.
  [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  [self updateFractionVisibleOfLastItem];
}

- (void)insertItem:(TabSwitcherItem*)item
           atIndex:(NSUInteger)index
    selectedItemID:(NSString*)selectedItemID {
  // Consistency check: |item|'s ID is not in |items|.
  // (using DCHECK rather than DCHECK_EQ to avoid a checked_cast on NSNotFound).
  DCHECK([self indexOfItemWithID:item.identifier] == NSNotFound);
  auto modelUpdates = ^{
    [self.items insertObject:item atIndex:index];
    self.selectedItemID = selectedItemID;
    self.lastInsertedItemID = item.identifier;
    [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  };
  auto collectionViewUpdates = ^{
    [self removeEmptyStateAnimated:YES];
    [self.collectionView insertItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
  };
  NSString* previouslySelectedItemID = self.selectedItemID;
  auto completion = ^(BOOL finished) {
    [self.collectionView
        deselectItemAtIndexPath:CreateIndexPath([self
                                    indexOfItemWithID:previouslySelectedItemID])
                       animated:YES];
    UICollectionViewScrollPosition scrollPosition =
        (self.currentLayout == self.horizontalLayout)
            ? UICollectionViewScrollPositionCenteredHorizontally
            : UICollectionViewScrollPositionNone;
    [self.collectionView
        selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                     animated:YES
               scrollPosition:scrollPosition];
    [self.delegate gridViewController:self didChangeItemCount:self.items.count];
    [self updateFractionVisibleOfLastItem];
  };

  [self performModelUpdates:modelUpdates
                collectionViewUpdates:collectionViewUpdates
                   useSpringAnimation:self.currentLayout ==
                                      self.horizontalLayout
      collectionViewUpdatesCompletion:completion];

  [self updateVisibleCellZIndex];
  [self updateVisibleCellIdentifiers];
}

- (void)removeItemWithID:(NSString*)removedItemID
          selectedItemID:(NSString*)selectedItemID {
  NSUInteger index = [self indexOfItemWithID:removedItemID];
  auto modelUpdates = ^{
    [self.items removeObjectAtIndex:index];
    self.selectedItemID = selectedItemID;
    [self deselectItemWithIDForEditing:removedItemID];
    [self.delegate gridViewController:self didChangeItemCount:self.items.count];
  };
  auto collectionViewUpdates = ^{
    [self.collectionView deleteItemsAtIndexPaths:@[ CreateIndexPath(index) ]];
    if (self.items.count == 0) {
      [self animateEmptyStateIn];
    }
  };
  auto completion = ^(BOOL finished) {
    if (self.items.count > 0) {
      [self.collectionView
          selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                       animated:YES
                 scrollPosition:UICollectionViewScrollPositionNone];
    }
    [self.delegate gridViewController:self didChangeItemCount:self.items.count];
    [self updateFractionVisibleOfLastItem];
  };
  [self performModelUpdates:modelUpdates
                collectionViewUpdates:collectionViewUpdates
                   useSpringAnimation:NO
      collectionViewUpdatesCompletion:completion];

  [self updateVisibleCellZIndex];
  [self updateVisibleCellIdentifiers];
}

- (void)selectItemWithID:(NSString*)selectedItemID {
  if (self.selectedItemID == selectedItemID)
    return;

  [self.collectionView
      deselectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                     animated:YES];
  self.selectedItemID = selectedItemID;
  [self.collectionView
      selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                   animated:YES
             scrollPosition:UICollectionViewScrollPositionNone];
}

- (void)replaceItemID:(NSString*)itemID withItem:(TabSwitcherItem*)item {
  if ([self indexOfItemWithID:itemID] == NSNotFound)
    return;
  // Consistency check: |item|'s ID is either |itemID| or not in |items|.
  DCHECK([item.identifier isEqualToString:itemID] ||
         [self indexOfItemWithID:item.identifier] == NSNotFound);
  NSUInteger index = [self indexOfItemWithID:itemID];
  self.items[index] = item;
  GridCell* cell = base::mac::ObjCCastStrict<GridCell>(
      [self.collectionView cellForItemAtIndexPath:CreateIndexPath(index)]);
  // |cell| may be nil if it is scrolled offscreen.
  if (cell)
    [self configureCell:cell withItem:item];
}

- (void)moveItemWithID:(NSString*)itemID toIndex:(NSUInteger)toIndex {
  NSUInteger fromIndex = [self indexOfItemWithID:itemID];
  // If this move would be a no-op, early return and avoid spurious UI updates.
  if (fromIndex == toIndex)
    return;
  auto modelUpdates = ^{
    TabSwitcherItem* item = self.items[fromIndex];
    [self.items removeObjectAtIndex:fromIndex];
    [self.items insertObject:item atIndex:toIndex];
  };
  auto collectionViewUpdates = ^{
    [self.collectionView moveItemAtIndexPath:CreateIndexPath(fromIndex)
                                 toIndexPath:CreateIndexPath(toIndex)];
  };
  auto completion = ^(BOOL finished) {
    [self.collectionView
        selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                     animated:YES
               scrollPosition:UICollectionViewScrollPositionNone];
  };
  [self performModelUpdates:modelUpdates
                collectionViewUpdates:collectionViewUpdates
                   useSpringAnimation:NO
      collectionViewUpdatesCompletion:completion];

  [self updateVisibleCellZIndex];
  [self updateVisibleCellIdentifiers];
}

- (void)dismissModals {
  ios::provider::DismissModalsForCollectionView(self.collectionView);
}

#pragma mark - LayoutSwitcher

- (void)willTransitionToLayout:(LayoutSwitcherState)nextState
                    completion:
                        (void (^)(BOOL completed, BOOL finished))completion {
  FlowLayout* nextLayout;
  BOOL thumbStripDismissEnabled;
  switch (nextState) {
    case LayoutSwitcherState::Horizontal:
      nextLayout = self.horizontalLayout;
      thumbStripDismissEnabled = YES;
      break;
    case LayoutSwitcherState::Grid:
      nextLayout = self.gridLayout;
      thumbStripDismissEnabled = NO;
      break;
  }

  // If the dismiss recognizer needs to be disabled, do it now so the user won't
  // trigger it during the transformation.
  if (!thumbStripDismissEnabled)
    self.thumbStripDismissRecognizer.enabled = NO;

  __weak __typeof(self) weakSelf = self;
  auto completionBlock = ^(BOOL completed, BOOL finished) {
    weakSelf.collectionView.scrollEnabled = YES;
    weakSelf.currentLayout = nextLayout;
    if (thumbStripDismissEnabled)
      self.thumbStripDismissRecognizer.enabled = YES;
    if (completion) {
      completion(completed, finished);
    }
  };
  self.gridHorizontalTransitionLayout = [self.collectionView
      startInteractiveTransitionToCollectionViewLayout:nextLayout
                                            completion:completionBlock];

  // Stops collectionView scrolling when the animation starts.
  [self.collectionView setContentOffset:self.collectionView.contentOffset
                               animated:NO];
}

- (void)didUpdateTransitionLayoutProgress:(CGFloat)progress {
  self.gridHorizontalTransitionLayout.transitionProgress = progress;
}

- (void)didTransitionToLayoutSuccessfully:(BOOL)success {
  if (success) {
    [self.collectionView finishInteractiveTransition];
  } else {
    [self.collectionView cancelInteractiveTransition];
  }
}

#pragma mark - Private properties

- (NSUInteger)selectedIndex {
  return [self indexOfItemWithID:self.selectedItemID];
}

- (CGFloat)offsetPastEndOfScrollView {
  CGFloat offset;
  if (self.currentLayout == self.horizontalLayout) {
    if (UseRTLLayout()) {
      offset = -self.collectionView.contentOffset.x;
    } else {
      // Use collectionViewLayout.collectionViwContentSize because it has the
      // correct size during a batch update.
      offset = self.collectionView.contentOffset.x +
               self.collectionView.frame.size.width -
               self.collectionView.collectionViewLayout
                   .collectionViewContentSize.width;
    }
  } else {
    DCHECK_EQ(self.gridLayout, self.currentLayout);
    // Use collectionViewLayout.collectionViwContentSize because it has the
    // correct size during a batch update.
    offset = self.collectionView.contentOffset.y +
             self.collectionView.frame.size.height -
             self.collectionView.collectionViewLayout.collectionViewContentSize
                 .width;
  }
  return offset;
}

- (void)setFractionVisibleOfLastItem:(CGFloat)fractionVisibleOfLastItem {
  if (fractionVisibleOfLastItem == _fractionVisibleOfLastItem)
    return;
  _fractionVisibleOfLastItem = fractionVisibleOfLastItem;

  if (self.currentLayout == self.horizontalLayout) {
    [self.delegate didChangeLastItemVisibilityInGridViewController:self];
  } else {
    DCHECK_EQ(self.gridLayout, self.currentLayout);
    // No-op because behaviour of the tab grid's bottom toolbar when the plus
    // sign cell is visible hasn't been decided yet. TODO(crbug.com/1146130)
  }
}

- (void)setCurrentLayout:(FlowLayout*)currentLayout {
  _currentLayout = currentLayout;
  [self updateFractionVisibleOfLastItem];

  // The collection view should only always bounce horizonal when in horizontal
  // layout.
  self.collectionView.alwaysBounceHorizontal =
      self.currentLayout == self.horizontalLayout;
}

#pragma mark - Private

// Checks whether |indexPath| corresponds to the index path of the plus sign
// cell. The plus sign cell is the last cell in the collection view after all
// the items.
- (BOOL)isIndexPathForPlusSignCell:(NSIndexPath*)indexPath {
  // When items are dragged from another collection, the count of cells in the
  // collectionView is increased before self.items.count increases. That's what
  // happens when the UICollectionViewDelegate's method
  // |targetIndexPathForMoveFromItemAtIndexPath:toProposedIndexPath:| gets
  // called, and that's why indexPath.item is not being compared to
  // self.items.count here.
  return self.thumbStripEnabled &&
         indexPath.item == [self.collectionView numberOfItemsInSection:0] - 1;
}

// Performs model updates and view updates together.
- (void)performModelUpdates:(ProceduralBlock)modelUpdates
              collectionViewUpdates:(ProceduralBlock)collectionViewUpdates
                 useSpringAnimation:(BOOL)useSpringAnimation
    collectionViewUpdatesCompletion:
        (ProceduralBlockWithBool)collectionViewUpdatesCompletion {
  auto batchUpdates = ^(void) {
    [self.collectionView
        performBatchUpdates:^{
          self.updating = YES;
          // Synchronize model and view updates.
          modelUpdates();
          collectionViewUpdates();
        }
        completion:^(BOOL completed) {
          collectionViewUpdatesCompletion(completed);
          self.updating = NO;
        }];
  };

  if (useSpringAnimation) {
    [UIView animateWithDuration:kSpringAnimationDuration
                          delay:0
         usingSpringWithDamping:kSpringAnimationDamping
          initialSpringVelocity:kSpringAnimationInitialVelocity
                        options:0
                     animations:batchUpdates
                     completion:nil];
  } else {
    batchUpdates();
  }
}

// Returns the index in |self.items| of the first item whose identifier is
// |identifier|.
- (NSUInteger)indexOfItemWithID:(NSString*)identifier {
  auto selectedTest =
      ^BOOL(TabSwitcherItem* item, NSUInteger index, BOOL* stop) {
        return [item.identifier isEqualToString:identifier];
      };
  return [self.items indexOfObjectPassingTest:selectedTest];
}

// Configures |cell|'s title synchronously, and favicon and snapshot
// asynchronously with information from |item|. Updates the |cell|'s theme to
// this view controller's theme. This view controller becomes the delegate for
// the cell.
- (void)configureCell:(GridCell*)cell withItem:(TabSwitcherItem*)item {
  DCHECK(cell);
  DCHECK(item);
  cell.delegate = self;
  cell.theme = self.theme;
  cell.itemIdentifier = item.identifier;
  cell.title = item.title;
  cell.titleHidden = item.hidesTitle;
  if (self.mode == TabGridModeSelection) {
    if ([self isItemWithIDSelectedForEditing:item.identifier]) {
      cell.state = GridCellStateEditingSelected;
    } else {
      cell.state = GridCellStateEditingUnselected;
    }
  } else {
    cell.state = GridCellStateNotEditing;
  }
  NSString* itemIdentifier = item.identifier;
  [self.imageDataSource faviconForIdentifier:itemIdentifier
                                  completion:^(UIImage* icon) {
                                    // Only update the icon if the cell is not
                                    // already reused for another item.
                                    if (cell.itemIdentifier == itemIdentifier)
                                      cell.icon = icon;
                                  }];
  [self.imageDataSource snapshotForIdentifier:itemIdentifier
                                   completion:^(UIImage* snapshot) {
                                     // Only update the icon if the cell is not
                                     // already reused for another item.
                                     if (cell.itemIdentifier == itemIdentifier)
                                       cell.snapshot = snapshot;
                                   }];
}

// Tells the delegate that the user tapped the item with identifier
// corresponding to |indexPath|.
- (void)tappedItemAtIndexPath:(NSIndexPath*)indexPath {
  if ([self isIndexPathForPlusSignCell:indexPath]) {
    [self.delegate didTapPlusSignInGridViewController:self];
    return;
  }

  // Speculative fix for crbug.com/1134663, where this method is called while
  // updates from a tab insertion are processing.
  if (self.updating)
    return;

  NSUInteger index = base::checked_cast<NSUInteger>(indexPath.item);
  DCHECK_LT(index, self.items.count);

  // crbug.com/1163238: In the wild, the above DCHECK condition is hit. This
  // might be a case of the UI sending touch events after the model has updated.
  // In this case, just no-op; if the user has to tap again to activate a tab,
  // that's better than a crash.
  if (index >= self.items.count)
    return;

  NSString* itemID = self.items[index].identifier;
  if (_mode == TabGridModeSelection) {
    if ([self isItemWithIDSelectedForEditing:itemID]) {
      [self deselectItemWithIDForEditing:itemID];
    } else {
      [self selectItemWithIDForEditing:itemID];
    }
    // Dragging multiple tabs to reorder them is not supported. So there is no
    // need to enable dragging when multiple items are selected in devices that
    // don't support multiple windows.
    if (self.selectedItemIDsForEditing.count > 1 &&
        !base::ios::IsMultipleScenesSupported()) {
      self.collectionView.dragInteractionEnabled = NO;
    } else {
      self.collectionView.dragInteractionEnabled = YES;
    }
    [UIView performWithoutAnimation:^{
      [self.collectionView reloadItemsAtIndexPaths:@[ indexPath ]];
    }];
  }

  [self.delegate gridViewController:self didSelectItemWithID:itemID];
}

// Animates the empty state into view.
- (void)animateEmptyStateIn {
  // TODO(crbug.com/820410) : Polish the animation, and put constants where they
  // belong.
  [self.emptyStateAnimator stopAnimation:YES];
  self.emptyStateAnimator = [[UIViewPropertyAnimator alloc]
      initWithDuration:1.0 - self.emptyStateView.alpha
          dampingRatio:1.0
            animations:^{
              self.emptyStateView.alpha = 1.0;
              self.emptyStateView.transform = CGAffineTransformIdentity;
            }];
  [self.emptyStateAnimator startAnimation];
}

// Removes the empty state out of view, with animation if |animated| is YES.
- (void)removeEmptyStateAnimated:(BOOL)animated {
  // TODO(crbug.com/820410) : Polish the animation, and put constants where they
  // belong.
  [self.emptyStateAnimator stopAnimation:YES];
  auto removeEmptyState = ^{
    self.emptyStateView.alpha = 0.0;
    self.emptyStateView.transform = CGAffineTransformScale(
        CGAffineTransformIdentity, /*sx=*/0.9, /*sy=*/0.9);
  };
  if (animated) {
    self.emptyStateAnimator = [[UIViewPropertyAnimator alloc]
        initWithDuration:self.emptyStateView.alpha
            dampingRatio:1.0
              animations:removeEmptyState];
    [self.emptyStateAnimator startAnimation];
  } else {
    removeEmptyState();
  }
}

// Updates the value stored in |fractionVisibleOfLastItem|.
- (void)updateFractionVisibleOfLastItem {
  CGFloat offset = self.offsetPastEndOfScrollView;
  self.fractionVisibleOfLastItem = base::clamp<CGFloat>(
      1 - offset / kScrollThresholdForPlusSignButtonHide, 0, 1);
}

// Updates the ZIndex of the visible cells to have the cells of the upper rows
// be above the cell of the lower rows.
- (void)updateVisibleCellZIndex {
  for (NSIndexPath* indexPath in self.collectionView
           .indexPathsForVisibleItems) {
    // Set the z index of cells so that lower rows are moving behind the upper
    // rows during transitions between grid and horizontal layouts.
    UICollectionViewCell* cell =
        [self.collectionView cellForItemAtIndexPath:indexPath];
    cell.layer.zPosition = self.items.count - indexPath.item;
  }
}

// Update visible cells identifier, following a reorg of cells.
- (void)updateVisibleCellIdentifiers {
  for (NSIndexPath* indexPath in self.collectionView
           .indexPathsForVisibleItems) {
    UICollectionViewCell* cell =
        [self.collectionView cellForItemAtIndexPath:indexPath];
    NSUInteger itemIndex = base::checked_cast<NSUInteger>(indexPath.item);
    cell.accessibilityIdentifier = [NSString
        stringWithFormat:@"%@%ld", kGridCellIdentifierPrefix, itemIndex];
  }
}

#pragma mark - Public Editing Mode Selection
- (void)selectAllItemsForEditing {
  if (_mode != TabGridModeSelection) {
    return;
  }

  for (TabSwitcherItem* item in self.items) {
    [self selectItemWithIDForEditing:item.identifier];
  }
  [self.collectionView reloadData];
}

- (void)deselectAllItemsForEditing {
  if (_mode != TabGridModeSelection) {
    return;
  }

  for (TabSwitcherItem* item in self.items) {
    [self deselectItemWithIDForEditing:item.identifier];
  }
  [self.collectionView reloadData];
}

- (NSArray<NSString*>*)selectedItemIDsForEditing {
  return [self.selectedEditingItemIDs allObjects];
}

- (NSArray<NSString*>*)selectedShareableItemIDsForEditing {
  return [self.selectedSharableEditingItemIDs allObjects];
}

- (BOOL)allItemsSelectedForEditing {
  return _mode == TabGridModeSelection &&
         self.items.count == self.selectedEditingItemIDs.count;
}

#pragma mark - Private Editing Mode Selection

- (BOOL)isItemWithIDSelectedForEditing:(NSString*)identifier {
  return [self.selectedEditingItemIDs containsObject:identifier];
}

- (void)selectItemWithIDForEditing:(NSString*)identifier {
  [self.selectedEditingItemIDs addObject:identifier];
  if ([self.shareableItemsProvider isItemWithIdentifierSharable:identifier]) {
    [self.selectedSharableEditingItemIDs addObject:identifier];
  }
}

- (void)deselectItemWithIDForEditing:(NSString*)identifier {
  [self.selectedEditingItemIDs removeObject:identifier];
  [self.selectedSharableEditingItemIDs removeObject:identifier];
}

#pragma mark - ThumbStripSupporting

- (void)thumbStripEnabledWithPanHandler:
    (ViewRevealingVerticalPanHandler*)panHandler {
  DCHECK(!self.thumbStripEnabled);

  UICollectionView* collectionView = self.collectionView;

  // Make sure the collection view isn't in the middle of a transition.
  DCHECK(![collectionView.collectionViewLayout
      isKindOfClass:[UICollectionViewTransitionLayout class]]);

  // Install tap gesture recognizer in collection to handle user taps on the
  // thumb strip background, assuming it hasn't already been installed.

  if (!self.thumbStripDismissRecognizer) {
    self.thumbStripDismissRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(handleThumbStripBackgroundTapGesture:)];
    DCHECK(collectionView.backgroundView);
    [collectionView.backgroundView
        addGestureRecognizer:self.thumbStripDismissRecognizer];
  }

  if (panHandler.currentState == ViewRevealState::Revealed) {
    self.thumbStripDismissRecognizer.enabled = NO;
    collectionView.collectionViewLayout = self.gridLayout;
    self.currentLayout = self.gridLayout;
  } else {
    self.thumbStripDismissRecognizer.enabled = YES;
    collectionView.collectionViewLayout = self.horizontalLayout;
    self.currentLayout = self.horizontalLayout;
  }

  [collectionView.collectionViewLayout invalidateLayout];
  [collectionView reloadData];
  _thumbStripEnabled = YES;
}

- (void)thumbStripDisabled {
  DCHECK(self.thumbStripEnabled);

  UICollectionView* collectionView = self.collectionView;
  FlowLayout* gridLayout = self.gridLayout;

  // Make sure the collection view isn't in the middle of a transition.
  DCHECK(![collectionView.collectionViewLayout
      isKindOfClass:[UICollectionViewTransitionLayout class]]);

  [collectionView.backgroundView
      removeGestureRecognizer:self.thumbStripDismissRecognizer];
  self.thumbStripDismissRecognizer = nil;

  collectionView.collectionViewLayout = gridLayout;
  self.currentLayout = gridLayout;

  [collectionView.collectionViewLayout invalidateLayout];
  [collectionView reloadData];

  _thumbStripEnabled = NO;
}

#pragma mark-- Thumbstrip tap dismiss handling

- (void)handleThumbStripBackgroundTapGesture:(UIGestureRecognizer*)recognizer {
  if (recognizer.state != UIGestureRecognizerStateEnded)
    return;

  UICollectionView* collectionView = self.collectionView;
  CGPoint tapLocation = [recognizer locationInView:collectionView];

  // Check that the tap wasn't to the leading side of the trailing edge of
  // the last cell. This should exclude any cells in the collection view as
  // well as the gaps between them.
  NSIndexPath* lastCellIndex =
      CreateIndexPath([collectionView numberOfItemsInSection:0] - 1);
  UICollectionViewCell* lastCell =
      [collectionView cellForItemAtIndexPath:lastCellIndex];
  if (lastCell) {
    CGRect lastCellFrame = [recognizer.view convertRect:lastCell.bounds
                                               fromView:lastCell];
    if (EdgeLeadsEdge(tapLocation.x, CGRectGetTrailingEdge(lastCellFrame)))
      return;
  }

  // Handle the tap.
  [self.thumbStripHandler closeThumbStrip];
}

@end
