// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller.h"

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/ios/block_types.h"
#import "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/numerics/ranges.h"
#import "base/numerics/safe_conversions.h"
#include "ios/chrome/browser/drag_and_drop/drag_and_drop_flag.h"
#include "ios/chrome/browser/procedural_block_types.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_drag_drop_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_empty_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_image_data_source.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/horizontal_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/plus_sign_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/grid_transition_layout.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* const kCellIdentifier = @"GridCellIdentifier";
NSString* const kPlusSignCellIdentifier = @"PlusSignCellIdentifier";
// Creates an NSIndexPath with |index| in section 0.
NSIndexPath* CreateIndexPath(NSInteger index) {
  return [NSIndexPath indexPathForItem:index inSection:0];
}
}  // namespace

#if defined(__IPHONE_13_4)
@interface GridViewController (Pointer) <UIPointerInteractionDelegate>
@end
#endif  // defined(__IPHONE_13_4)

@interface GridViewController () <GridCellDelegate,
                                  UICollectionViewDataSource,
                                  UICollectionViewDelegate,
                                  UICollectionViewDragDelegate,
                                  UICollectionViewDropDelegate>
// A collection view of items in a grid format.
@property(nonatomic, weak) UICollectionView* collectionView;
// The local model backing the collection view.
@property(nonatomic, strong) NSMutableArray<GridItem*>* items;
// Identifier of the selected item. This value is disregarded if |self.items| is
// empty. This bookkeeping is done to set the correct selection on
// |-viewWillAppear:|.
@property(nonatomic, copy) NSString* selectedItemID;
// Index of the selected item in |items|.
@property(nonatomic, readonly) NSUInteger selectedIndex;
// ID of the last item to be inserted. This is used to track if the active tab
// was newly created when building the animation layout for transitions.
@property(nonatomic, copy) NSString* lastInsertedItemID;
// The gesture recognizer used for interactive item reordering.
@property(nonatomic, strong)
    UILongPressGestureRecognizer* itemReorderRecognizer;
// The touch location of an interactively reordering item, expressed as an
// offset from its center. This is subtracted from the location that is passed
// to -updateInteractiveMovementTargetPosition: so that the moving item will
// keep them same relative location to the user's touch.
@property(nonatomic, assign) CGPoint itemReorderTouchPoint;
// Animator to show or hide the empty state.
@property(nonatomic, strong) UIViewPropertyAnimator* emptyStateAnimator;
// The current layout for the tab switcher.
@property(nonatomic, strong) FlowLayout* currentLayout;
// The layout for the tab grid.
@property(nonatomic, strong) GridLayout* gridLayout;
// The layout for the thumb strip.
@property(nonatomic, strong) HorizontalLayout* horizontalLayout;
// The layout used while the grid is being reordered.
@property(nonatomic, strong) UICollectionViewLayout* reorderingLayout;
// The layout used while the thumb strip is being reordered.
@property(nonatomic, strong) UICollectionViewLayout* horizontalReorderingLayout;
// YES if, when reordering is enabled, the order of the cells has changed.
@property(nonatomic, assign) BOOL hasChangedOrder;
// By how much the user scrolled past the view's content size. A negative value
// means the user hasn't scrolled past the end of the scroll view.
@property(nonatomic, assign, readonly) CGFloat offsetPastEndOfScrollView;
#if defined(__IPHONE_13_4)
// Cells for which pointer interactions have been added. Pointer interactions
// should only be added to displayed cells (not transition cells). This is only
// expected to get as large as the number of reusable cells in memory.
@property(nonatomic, strong)
    NSHashTable<UICollectionViewCell*>* pointerInteractionCells API_AVAILABLE(
        ios(13.4));
#endif  // defined(__IPHONE_13_4)
// The transition layout either from grid to horizontal layout or from
// horizontal to grid layout.
@property(nonatomic, strong)
    UICollectionViewTransitionLayout* gridHorizontalTransitionLayout;

// YES while batch updates and the batch update completion are being performed.
@property(nonatomic) BOOL updating;
@end

@implementation GridViewController

- (instancetype)init {
  if (self = [super init]) {
    _items = [[NSMutableArray<GridItem*> alloc] init];
    _showsSelectionUpdates = YES;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  self.horizontalLayout = [[HorizontalLayout alloc] init];
  self.gridLayout = [[GridLayout alloc] init];
  if (IsThumbStripEnabled()) {
    self.currentLayout = self.horizontalLayout;
  } else {
    self.currentLayout = self.gridLayout;
  }

  UICollectionView* collectionView =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:self.currentLayout];
  [collectionView registerClass:[GridCell class]
      forCellWithReuseIdentifier:kCellIdentifier];
  [collectionView registerClass:[PlusSignCell class]
      forCellWithReuseIdentifier:kPlusSignCellIdentifier];
  collectionView.dataSource = self;
  collectionView.delegate = self;
  collectionView.backgroundView = [[UIView alloc] init];
  collectionView.backgroundView.backgroundColor =
      [UIColor colorNamed:kGridBackgroundColor];
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

  if (DragAndDropIsEnabled()) {
    collectionView.dragDelegate = self;
    collectionView.dropDelegate = self;
    collectionView.dragInteractionEnabled = YES;
  } else {
    self.reorderingLayout = [[GridReorderingLayout alloc] init];
    if (IsThumbStripEnabled()) {
      self.horizontalReorderingLayout =
          [[HorizontalReorderingLayout alloc] init];
    }
    self.itemReorderRecognizer = [[UILongPressGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(handleItemReorderingWithGesture:)];
    // The collection view cells will by default get touch events in parallel
    // with the reorder recognizer. When this happens, long-pressing on a
    // non-selected cell will cause the selected cell to briefly become
    // unselected and then selected again. To avoid this, the recognizer delays
    // touchesBegan: calls until it fails to recognize a long-press.
    self.itemReorderRecognizer.delaysTouchesBegan = YES;
    [collectionView addGestureRecognizer:self.itemReorderRecognizer];
  }

#if defined(__IPHONE_13_4)
  if (@available(iOS 13.4, *)) {
    if (base::FeatureList::IsEnabled(kPointerSupport)) {
      self.pointerInteractionCells =
          [NSHashTable<UICollectionViewCell*> weakObjectsHashTable];
    }
  }
#endif  // defined(__IPHONE_13_4)
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self contentWillAppearAnimated:animated];
}

- (void)viewWillDisappear:(BOOL)animated {
  [self contentWillDisappear];
  [super viewWillDisappear:animated];
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
  [self.collectionView selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                                    animated:NO
                              scrollPosition:UICollectionViewScrollPositionTop];
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
  if (IsThumbStripEnabled()) {
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

    GridItem* item = self.items[itemIndex];
    cell =
        [collectionView dequeueReusableCellWithReuseIdentifier:kCellIdentifier
                                                  forIndexPath:indexPath];
    cell.accessibilityIdentifier = [NSString
        stringWithFormat:@"%@%ld", kGridCellIdentifierPrefix, itemIndex];
    GridCell* gridCell = base::mac::ObjCCastStrict<GridCell>(cell);
    [self configureCell:gridCell withItem:item];
  }
  // Set the z index of cells so that lower rows are superposed during
  // transitions between grid and horizontal layouts.
  cell.layer.zPosition = itemIndex;

#if defined(__IPHONE_13_4)
  if (@available(iOS 13.4, *)) {
    if (base::FeatureList::IsEnabled(kPointerSupport)) {
      if (![self.pointerInteractionCells containsObject:cell]) {
        [cell addInteraction:[[UIPointerInteraction alloc]
                                 initWithDelegate:self]];
        // |self.pointerInteractionCells| is only expected to get as large as
        // the number of reusable cells in memory.
        [self.pointerInteractionCells addObject:cell];
      }
    }
  }
#endif  // defined(__IPHONE_13_4)
  return cell;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    canMoveItemAtIndexPath:(NSIndexPath*)indexPath {
  if ([self isIndexPathForPlusSignCell:indexPath]) {
    // The PlusSignCell is at the end of the collection and should not be moved.
    return NO;
  }
  return indexPath && self.items.count > 1;
}

- (void)collectionView:(UICollectionView*)collectionView
    moveItemAtIndexPath:(NSIndexPath*)sourceIndexPath
            toIndexPath:(NSIndexPath*)destinationIndexPath {
  NSUInteger source = base::checked_cast<NSUInteger>(sourceIndexPath.item);
  NSUInteger destination =
      base::checked_cast<NSUInteger>(destinationIndexPath.item);
  // Update |items| before informing the delegate, so the state of the UI
  // is correctly represented before any updates occur.
  GridItem* item = self.items[source];
  [self.items removeObjectAtIndex:source];
  [self.items insertObject:item atIndex:destination];
  self.hasChangedOrder = YES;
  [self.delegate gridViewController:self
                  didMoveItemWithID:item.identifier
                            toIndex:destination];
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

#if defined(__IPHONE_13_4)
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
#endif  // defined(__IPHONE_13_4)

#pragma mark - UICollectionViewDragDelegate

- (NSArray<UIDragItem*>*)collectionView:(UICollectionView*)collectionView
           itemsForBeginningDragSession:(id<UIDragSession>)session
                            atIndexPath:(NSIndexPath*)indexPath {
  if ([self isIndexPathForPlusSignCell:indexPath]) {
    // Return an empty array because the plus sign cell should not be dragged.
    return @[];
  }
  GridItem* item = self.items[indexPath.item];
  return @[ [self.dragDropHandler dragItemForItemWithID:item.identifier] ];
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
  return session.items.count == 1U;
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
  id<UICollectionViewDropItem> item = coordinator.items.firstObject;

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
  if (IsThumbStripEnabled()) {
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
    return;
  }

  // Drop asynchronously if local object is not available.
  UICollectionViewDropPlaceholder* placeholder =
      [[UICollectionViewDropPlaceholder alloc]
          initWithInsertionIndexPath:dropIndexPath
                     reuseIdentifier:kCellIdentifier];
  placeholder.cellUpdateHandler = ^(UICollectionViewCell* placeholderCell) {
    GridCell* gridCell = base::mac::ObjCCastStrict<GridCell>(placeholderCell);
    gridCell.theme = self.theme;
  };
  placeholder.previewParametersProvider =
      ^UIDragPreviewParameters*(UICollectionViewCell* placeholderCell) {
    GridCell* gridCell = base::mac::ObjCCastStrict<GridCell>(placeholderCell);
    return gridCell.dragPreviewParameters;
  };

  id<UICollectionViewDropPlaceholderContext> context =
      [coordinator dropItem:item.dragItem toPlaceholder:placeholder];
  [self.dragDropHandler dropItemFromProvider:item.dragItem.itemProvider
                                     toIndex:destinationIndex
                          placeholderContext:context];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidChangeAdjustedContentInset:(UIScrollView*)scrollView {
  self.emptyStateView.scrollViewContentInsets = scrollView.contentInset;
}

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  if (!IsThumbStripEnabled())
    return;
  [self updateFractionVisibleOfLastItem];
}

#pragma mark - GridCellDelegate

- (void)closeButtonTappedForCell:(GridCell*)cell {
  if (!DragAndDropIsEnabled()) {
    // Disable the reordering recognizer to cancel any in-flight reordering. The
    // DCHECK below ensures that the gesture is re-enabled after being cancelled
    // in |-handleItemReorderingWithGesture:|.
    if (self.itemReorderRecognizer.state != UIGestureRecognizerStatePossible) {
      self.itemReorderRecognizer.enabled = NO;
      DCHECK(self.itemReorderRecognizer.enabled);
    }
  }

  [self.delegate gridViewController:self
                 didCloseItemWithID:cell.itemIdentifier];
  // Record when a tab is closed via the X.
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCloseControlTapped"));
}

#pragma mark - GridConsumer

- (void)populateItems:(NSArray<GridItem*>*)items
       selectedItemID:(NSString*)selectedItemID {
#ifndef NDEBUG
  // Consistency check: ensure no IDs are duplicated.
  NSMutableSet<NSString*>* identifiers = [[NSMutableSet alloc] init];
  for (GridItem* item in items) {
    [identifiers addObject:item.identifier];
  }
  CHECK_EQ(identifiers.count, items.count);
#endif

  self.items = [items mutableCopy];
  self.selectedItemID = selectedItemID;
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

- (void)insertItem:(GridItem*)item
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
    [self.collectionView
        selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                     animated:YES
               scrollPosition:UICollectionViewScrollPositionNone];
    [self.delegate gridViewController:self didChangeItemCount:self.items.count];
    [self updateFractionVisibleOfLastItem];
  };
  [self performModelUpdates:modelUpdates
                collectionViewUpdates:collectionViewUpdates
      collectionViewUpdatesCompletion:completion];
}

- (void)removeItemWithID:(NSString*)removedItemID
          selectedItemID:(NSString*)selectedItemID {
  if (!DragAndDropIsEnabled()) {
    // Disable the reordering recognizer to cancel any in-flight reordering. The
    // DCHECK below ensures that the gesture is re-enabled after being cancelled
    // in |-handleItemReorderingWithGesture:|.
    if (self.itemReorderRecognizer.state != UIGestureRecognizerStatePossible &&
        self.itemReorderRecognizer.state != UIGestureRecognizerStateCancelled) {
      self.itemReorderRecognizer.enabled = NO;
      DCHECK(self.itemReorderRecognizer.enabled);
    }
  }

  NSUInteger index = [self indexOfItemWithID:removedItemID];
  auto modelUpdates = ^{
    [self.items removeObjectAtIndex:index];
    self.selectedItemID = selectedItemID;
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
      collectionViewUpdatesCompletion:completion];
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

- (void)replaceItemID:(NSString*)itemID withItem:(GridItem*)item {
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
    GridItem* item = self.items[fromIndex];
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
      collectionViewUpdatesCompletion:completion];
}

#pragma mark - LayoutSwitcher

- (void)willTransitionToLayout:(LayoutSwitcherState)nextState
                    completion:
                        (void (^)(BOOL completed, BOOL finished))completion {
  FlowLayout* nextLayout;
  switch (nextState) {
    case LayoutSwitcherState::Horizontal:
      nextLayout = self.horizontalLayout;
      break;
    case LayoutSwitcherState::Grid:
      nextLayout = self.gridLayout;
      break;
  }
  auto completionBlock = ^(BOOL completed, BOOL finished) {
    completion(completed, finished);
    self.collectionView.scrollEnabled = YES;
    self.currentLayout = nextLayout;
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
    offset = self.collectionView.contentOffset.x +
             self.collectionView.frame.size.width -
             self.collectionView.contentSize.width;
  } else {
    DCHECK_EQ(self.gridLayout, self.currentLayout);
    offset = self.collectionView.contentOffset.y +
             self.collectionView.frame.size.height -
             self.collectionView.contentSize.height;
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
  return IsThumbStripEnabled() &&
         indexPath.item == [self.collectionView numberOfItemsInSection:0] - 1;
}

// Performs model updates and view updates together.
- (void)performModelUpdates:(ProceduralBlock)modelUpdates
              collectionViewUpdates:(ProceduralBlock)collectionViewUpdates
    collectionViewUpdatesCompletion:
        (ProceduralBlockWithBool)collectionViewUpdatesCompletion {
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
}

// Returns the index in |self.items| of the first item whose identifier is
// |identifier|.
- (NSUInteger)indexOfItemWithID:(NSString*)identifier {
  auto selectedTest = ^BOOL(GridItem* item, NSUInteger index, BOOL* stop) {
    return [item.identifier isEqualToString:identifier];
  };
  return [self.items indexOfObjectPassingTest:selectedTest];
}

// Configures |cell|'s title synchronously, and favicon and snapshot
// asynchronously with information from |item|. Updates the |cell|'s theme to
// this view controller's theme. This view controller becomes the delegate for
// the cell.
- (void)configureCell:(GridCell*)cell withItem:(GridItem*)item {
  DCHECK(cell);
  DCHECK(item);
  cell.delegate = self;
  cell.theme = self.theme;
  cell.itemIdentifier = item.identifier;
  cell.title = item.title;
  cell.titleHidden = item.hidesTitle;
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
  NSString* itemID = self.items[index].identifier;
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
  self.fractionVisibleOfLastItem = base::ClampToRange<CGFloat>(
      1 - offset / kScrollThresholdForPlusSignButtonHide, 0, 1);
}

#pragma mark - Custom Gesture-based Reordering

// Handle the long-press gesture used to reorder cells in the collection view.
- (void)handleItemReorderingWithGesture:(UIGestureRecognizer*)gesture {
  DCHECK(gesture == self.itemReorderRecognizer);
  CGPoint location = [gesture locationInView:self.collectionView];
  switch (gesture.state) {
    case UIGestureRecognizerStateBegan: {
      NSIndexPath* path =
          [self.collectionView indexPathForItemAtPoint:location];
      BOOL moving =
          [self.collectionView beginInteractiveMovementForItemAtIndexPath:path];
      if (!moving) {
        gesture.enabled = NO;
      } else {
        base::RecordAction(
            base::UserMetricsAction("MobileTabGridBeganReordering"));
        CGPoint cellCenter =
            [self.collectionView cellForItemAtIndexPath:path].center;
        self.itemReorderTouchPoint =
            CGPointMake(location.x - cellCenter.x, location.y - cellCenter.y);
        // Switch to the reordering layout.
        if (IsThumbStripEnabled() &&
            self.currentLayout == self.horizontalLayout) {
          [self.collectionView
              setCollectionViewLayout:self.horizontalReorderingLayout
                             animated:YES];
        } else {
          [self.collectionView setCollectionViewLayout:self.reorderingLayout
                                              animated:YES];
        }
        // Immediately update the position of the moving item; otherwise, the
        // collection view may relayout its subviews and briefly show the moving
        // item at position (0,0).
        [self updateItemReorderingForPosition:location];
      }
      break;
    }
    case UIGestureRecognizerStateChanged:
      // Offset the location so it's the touch point that moves, not the cell
      // center.
      [self updateItemReorderingForPosition:location];
      break;
    case UIGestureRecognizerStateEnded: {
      self.itemReorderTouchPoint = CGPointZero;
      // End the interactive movement and transition the layout back to the
      // defualt layout. These can't be simultaneous, because that will produce
      // a copy of the moving cell in its final position while it animates from
      // under thr user's touch. In order to fire the animated switch to the
      // defualt layout at the correct time, a CATransaction is used to wrap the
      // -endInteractiveMovement call which will generate the animations on the
      // moving cell. The -setCollectionViewLayout: call can then be added as a
      // completion block.
      [CATransaction begin];
      // Note: The completion block must be added *before* any animations are
      // added in the transaction.
      [CATransaction setCompletionBlock:^{
        [self.collectionView setCollectionViewLayout:self.currentLayout
                                            animated:YES];
      }];
      [self.collectionView endInteractiveMovement];
      [self recordInteractiveReordering];
      [CATransaction commit];
      break;
    }
    case UIGestureRecognizerStateCancelled:
      self.itemReorderTouchPoint = CGPointZero;
      [self.collectionView cancelInteractiveMovement];
      [self recordInteractiveReordering];
      [self.collectionView setCollectionViewLayout:self.currentLayout
                                          animated:YES];
      // Re-enable cancelled gesture.
      gesture.enabled = YES;
      break;
    case UIGestureRecognizerStatePossible:
    case UIGestureRecognizerStateFailed:
      NOTREACHED() << "Unexpected long-press recognizer state";
  }
}

- (void)updateItemReorderingForPosition:(CGPoint)position {
  CGPoint targetLocation =
      CGPointMake(position.x - self.itemReorderTouchPoint.x,
                  position.y - self.itemReorderTouchPoint.y);

  [self.collectionView updateInteractiveMovementTargetPosition:targetLocation];
}

- (void)recordInteractiveReordering {
  if (self.hasChangedOrder) {
    base::RecordAction(base::UserMetricsAction("MobileTabGridReordered"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridEndedWithoutReordering"));
  }
  self.hasChangedOrder = NO;
}

@end
