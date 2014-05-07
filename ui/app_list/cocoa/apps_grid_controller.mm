// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/apps_grid_controller.h"

#include "base/mac/foundation_util.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_model_observer.h"
#include "ui/app_list/app_list_view_delegate.h"
#import "ui/app_list/cocoa/apps_collection_view_drag_manager.h"
#import "ui/app_list/cocoa/apps_grid_view_item.h"
#import "ui/app_list/cocoa/apps_pagination_model_observer.h"
#include "ui/base/models/list_model_observer.h"

namespace {

// OSX app list has hardcoded rows and columns for now.
const int kFixedRows = 4;
const int kFixedColumns = 4;
const int kItemsPerPage = kFixedRows * kFixedColumns;

// Padding space in pixels for fixed layout.
const CGFloat kGridTopPadding = 1;
const CGFloat kLeftRightPadding = 21;
const CGFloat kScrollerPadding = 16;

// Preferred tile size when showing in fixed layout. These should be even
// numbers to ensure that if they are grown 50% they remain integers.
const CGFloat kPreferredTileWidth = 88;
const CGFloat kPreferredTileHeight = 98;

const CGFloat kViewWidth =
    kFixedColumns * kPreferredTileWidth + 2 * kLeftRightPadding;
const CGFloat kViewHeight = kFixedRows * kPreferredTileHeight;

const NSTimeInterval kScrollWhileDraggingDelay = 1.0;
NSTimeInterval g_scroll_duration = 0.18;

}  // namespace

@interface AppsGridController ()

- (void)scrollToPageWithTimer:(size_t)targetPage;
- (void)onTimer:(NSTimer*)theTimer;

// Cancel a currently running scroll animation.
- (void)cancelScrollAnimation;

// Index of the page with the most content currently visible.
- (size_t)nearestPageIndex;

// Bootstrap the views this class controls.
- (void)loadAndSetView;

- (void)boundsDidChange:(NSNotification*)notification;

// Action for buttons in the grid.
- (void)onItemClicked:(id)sender;

- (AppsGridViewItem*)itemAtPageIndex:(size_t)pageIndex
                         indexInPage:(size_t)indexInPage;

// Return the button of the selected item.
- (NSButton*)selectedButton;

// The scroll view holding the grid pages.
- (NSScrollView*)gridScrollView;

- (NSView*)pagesContainerView;

// Create any new pages after updating |items_|.
- (void)updatePages:(size_t)startItemIndex;

- (void)updatePageContent:(size_t)pageIndex
               resetModel:(BOOL)resetModel;

// Bridged methods for AppListItemListObserver.
- (void)listItemAdded:(size_t)index
                 item:(app_list::AppListItem*)item;

- (void)listItemRemoved:(size_t)index;

- (void)listItemMovedFromIndex:(size_t)fromIndex
                  toModelIndex:(size_t)toIndex;

// Moves the selection by |indexDelta| items.
- (BOOL)moveSelectionByDelta:(int)indexDelta;

@end

namespace app_list {

class AppsGridDelegateBridge : public AppListItemListObserver {
 public:
  AppsGridDelegateBridge(AppsGridController* parent) : parent_(parent) {}

 private:
  // Overridden from AppListItemListObserver:
  virtual void OnListItemAdded(size_t index, AppListItem* item) OVERRIDE {
    [parent_ listItemAdded:index
                      item:item];
  }
  virtual void OnListItemRemoved(size_t index, AppListItem* item) OVERRIDE {
    [parent_ listItemRemoved:index];
  }
  virtual void OnListItemMoved(size_t from_index,
                               size_t to_index,
                               AppListItem* item) OVERRIDE {
    [parent_ listItemMovedFromIndex:from_index
                       toModelIndex:to_index];
  }

  AppsGridController* parent_;  // Weak, owns us.

  DISALLOW_COPY_AND_ASSIGN(AppsGridDelegateBridge);
};

}  // namespace app_list

@interface PageContainerView : NSView;
@end

// The container view needs to flip coordinates so that it is laid out
// correctly whether or not there is a horizontal scrollbar.
@implementation PageContainerView

- (BOOL)isFlipped {
  return YES;
}

@end

@implementation AppsGridController

+ (void)setScrollAnimationDuration:(NSTimeInterval)duration {
  g_scroll_duration = duration;
}

+ (CGFloat)scrollerPadding {
  return kScrollerPadding;
}

@synthesize paginationObserver = paginationObserver_;

- (id)init {
  if ((self = [super init])) {
    bridge_.reset(new app_list::AppsGridDelegateBridge(self));
    NSSize cellSize = NSMakeSize(kPreferredTileWidth, kPreferredTileHeight);
    dragManager_.reset(
        [[AppsCollectionViewDragManager alloc] initWithCellSize:cellSize
                                                           rows:kFixedRows
                                                        columns:kFixedColumns
                                                 gridController:self]);
    pages_.reset([[NSMutableArray alloc] init]);
    items_.reset([[NSMutableArray alloc] init]);
    [self loadAndSetView];
    [self updatePages:0];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (NSCollectionView*)collectionViewAtPageIndex:(size_t)pageIndex {
  return [pages_ objectAtIndex:pageIndex];
}

- (size_t)pageIndexForCollectionView:(NSCollectionView*)page {
  for (size_t pageIndex = 0; pageIndex < [pages_ count]; ++pageIndex) {
    if (page == [self collectionViewAtPageIndex:pageIndex])
      return pageIndex;
  }
  return NSNotFound;
}

- (app_list::AppListModel*)model {
  return delegate_ ? delegate_->GetModel() : NULL;
}

- (void)setDelegate:(app_list::AppListViewDelegate*)newDelegate {
  if (delegate_) {
    app_list::AppListModel* oldModel = delegate_->GetModel();
    if (oldModel)
      oldModel->top_level_item_list()->RemoveObserver(bridge_.get());
  }

  // Since the old model may be getting deleted, and the AppKit objects might
  // be sitting in an NSAutoreleasePool, ensure there are no references to
  // the model.
  for (size_t i = 0; i < [items_ count]; ++i)
    [[self itemAtIndex:i] setModel:NULL];

  [items_ removeAllObjects];
  [self updatePages:0];
  [self scrollToPage:0];

  delegate_ = newDelegate;
  if (!delegate_)
    return;

  app_list::AppListModel* newModel = delegate_->GetModel();
  if (!newModel)
    return;

  newModel->top_level_item_list()->AddObserver(bridge_.get());
  for (size_t i = 0; i < newModel->top_level_item_list()->item_count(); ++i) {
    app_list::AppListItem* itemModel =
        newModel->top_level_item_list()->item_at(i);
    [items_ insertObject:[NSValue valueWithPointer:itemModel]
                 atIndex:i];
  }
  [self updatePages:0];
}

- (size_t)visiblePage {
  return visiblePage_;
}

- (void)activateSelection {
  [[self selectedButton] performClick:self];
}

- (size_t)pageCount {
  return [pages_ count];
}

- (size_t)itemCount {
  return [items_ count];
}

- (void)scrollToPage:(size_t)pageIndex {
  NSClipView* clipView = [[self gridScrollView] contentView];
  NSPoint newOrigin = [clipView bounds].origin;

  // Scrolling outside of this range is edge elasticity, which animates
  // automatically.
  if ((pageIndex == 0 && (newOrigin.x <= 0)) ||
      (pageIndex + 1 == [self pageCount] &&
          newOrigin.x >= pageIndex * kViewWidth)) {
    return;
  }

  // Clear any selection on the current page (unless it has been removed).
  if (visiblePage_ < [pages_ count]) {
    [[self collectionViewAtPageIndex:visiblePage_]
        setSelectionIndexes:[NSIndexSet indexSet]];
  }

  newOrigin.x = pageIndex * kViewWidth;
  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext] setDuration:g_scroll_duration];
  [[clipView animator] setBoundsOrigin:newOrigin];
  [NSAnimationContext endGrouping];
  animatingScroll_ = YES;
  targetScrollPage_ = pageIndex;
  [self cancelScrollTimer];
}

- (void)maybeChangePageForPoint:(NSPoint)locationInWindow {
  NSPoint pointInView = [[self view] convertPoint:locationInWindow
                                         fromView:nil];
  // Check if the point is outside the view on the left or right.
  if (pointInView.x <= 0 || pointInView.x >= NSWidth([[self view] bounds])) {
    size_t targetPage = visiblePage_;
    if (pointInView.x <= 0)
      targetPage -= targetPage != 0 ? 1 : 0;
    else
      targetPage += targetPage < [pages_ count] - 1 ? 1 : 0;
    [self scrollToPageWithTimer:targetPage];
    return;
  }

  if (paginationObserver_) {
    NSInteger segment =
        [paginationObserver_ pagerSegmentAtLocation:locationInWindow];
    if (segment >= 0 && static_cast<size_t>(segment) != targetScrollPage_) {
      [self scrollToPageWithTimer:segment];
      return;
    }
  }

  // Otherwise the point may have moved back into the view.
  [self cancelScrollTimer];
}

- (void)cancelScrollTimer {
  scheduledScrollPage_ = targetScrollPage_;
  [scrollWhileDraggingTimer_ invalidate];
}

- (void)scrollToPageWithTimer:(size_t)targetPage {
  if (targetPage == targetScrollPage_) {
    [self cancelScrollTimer];
    return;
  }

  if (targetPage == scheduledScrollPage_)
    return;

  scheduledScrollPage_ = targetPage;
  [scrollWhileDraggingTimer_ invalidate];
  scrollWhileDraggingTimer_.reset(
      [[NSTimer scheduledTimerWithTimeInterval:kScrollWhileDraggingDelay
                                        target:self
                                      selector:@selector(onTimer:)
                                      userInfo:nil
                                       repeats:NO] retain]);
}

- (void)onTimer:(NSTimer*)theTimer {
  if (scheduledScrollPage_ == targetScrollPage_)
    return;  // Already animating scroll.

  [self scrollToPage:scheduledScrollPage_];
}

- (void)cancelScrollAnimation {
  NSClipView* clipView = [[self gridScrollView] contentView];
  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext] setDuration:0];
  [[clipView animator] setBoundsOrigin:[clipView bounds].origin];
  [NSAnimationContext endGrouping];
  animatingScroll_ = NO;
}

- (size_t)nearestPageIndex {
  return lround(
      NSMinX([[[self gridScrollView] contentView] bounds]) / kViewWidth);
}

- (void)userScrolling:(BOOL)isScrolling {
  if (isScrolling) {
    if (animatingScroll_)
      [self cancelScrollAnimation];
  } else {
    [self scrollToPage:[self nearestPageIndex]];
  }
}

- (void)loadAndSetView {
  base::scoped_nsobject<PageContainerView> pagesContainer(
      [[PageContainerView alloc] initWithFrame:NSZeroRect]);

  NSRect scrollFrame = NSMakeRect(0, kGridTopPadding, kViewWidth,
                                  kViewHeight + kScrollerPadding);
  base::scoped_nsobject<ScrollViewWithNoScrollbars> scrollView(
      [[ScrollViewWithNoScrollbars alloc] initWithFrame:scrollFrame]);
  [scrollView setBorderType:NSNoBorder];
  [scrollView setLineScroll:kViewWidth];
  [scrollView setPageScroll:kViewWidth];
  [scrollView setDelegate:self];
  [scrollView setDocumentView:pagesContainer];
  [scrollView setDrawsBackground:NO];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(boundsDidChange:)
             name:NSViewBoundsDidChangeNotification
           object:[scrollView contentView]];

  [self setView:scrollView];
}

- (void)boundsDidChange:(NSNotification*)notification {
  size_t newPage = [self nearestPageIndex];
  if (newPage == visiblePage_) {
    [paginationObserver_ pageVisibilityChanged];
    return;
  }

  visiblePage_ = newPage;
  [paginationObserver_ selectedPageChanged:newPage];
  [paginationObserver_ pageVisibilityChanged];
}

- (void)onItemClicked:(id)sender {
  for (size_t i = 0; i < [items_ count]; ++i) {
    AppsGridViewItem* gridItem = [self itemAtIndex:i];
    if ([[gridItem button] isEqual:sender])
      [gridItem model]->Activate(0);
  }
}

- (AppsGridViewItem*)itemAtPageIndex:(size_t)pageIndex
                         indexInPage:(size_t)indexInPage {
  return base::mac::ObjCCastStrict<AppsGridViewItem>(
      [[self collectionViewAtPageIndex:pageIndex] itemAtIndex:indexInPage]);
}

- (AppsGridViewItem*)itemAtIndex:(size_t)itemIndex {
  const size_t pageIndex = itemIndex / kItemsPerPage;
  return [self itemAtPageIndex:pageIndex
                   indexInPage:itemIndex - pageIndex * kItemsPerPage];
}

- (NSUInteger)selectedItemIndex {
  NSCollectionView* page = [self collectionViewAtPageIndex:visiblePage_];
  NSUInteger indexOnPage = [[page selectionIndexes] firstIndex];
  if (indexOnPage == NSNotFound)
    return NSNotFound;

  return indexOnPage + visiblePage_ * kItemsPerPage;
}

- (NSButton*)selectedButton {
  NSUInteger index = [self selectedItemIndex];
  if (index == NSNotFound)
    return nil;

  return [[self itemAtIndex:index] button];
}

- (NSScrollView*)gridScrollView {
  return base::mac::ObjCCastStrict<NSScrollView>([self view]);
}

- (NSView*)pagesContainerView {
  return [[self gridScrollView] documentView];
}

- (void)updatePages:(size_t)startItemIndex {
  // Note there is always at least one page.
  size_t targetPages = 1;
  if ([items_ count] != 0)
    targetPages = ([items_ count] - 1) / kItemsPerPage + 1;

  const size_t currentPages = [self pageCount];
  // First see if the number of pages have changed.
  if (targetPages != currentPages) {
    if (targetPages < currentPages) {
      // Pages need to be removed.
      [pages_ removeObjectsInRange:NSMakeRange(targetPages,
                                               currentPages - targetPages)];
    } else {
      // Pages need to be added.
      for (size_t i = currentPages; i < targetPages; ++i) {
        NSRect pageFrame = NSMakeRect(
            kLeftRightPadding + kViewWidth * i, 0,
            kViewWidth, kViewHeight);
        [pages_ addObject:[dragManager_ makePageWithFrame:pageFrame]];
      }
    }

    [[self pagesContainerView] setSubviews:pages_];
    NSSize pagesSize = NSMakeSize(kViewWidth * targetPages, kViewHeight);
    [[self pagesContainerView] setFrameSize:pagesSize];
    [paginationObserver_ totalPagesChanged];
  }

  const size_t startPage = startItemIndex / kItemsPerPage;
  // All pages on or after |startPage| may need items added or removed.
  for (size_t pageIndex = startPage; pageIndex < targetPages; ++pageIndex) {
    [self updatePageContent:pageIndex
                 resetModel:YES];
  }
}

- (void)updatePageContent:(size_t)pageIndex
               resetModel:(BOOL)resetModel {
  NSCollectionView* pageView = [self collectionViewAtPageIndex:pageIndex];
  if (resetModel) {
    // Clear the models first, otherwise removed items could be autoreleased at
    // an unknown point in the future, when the model owner may have gone away.
    for (size_t i = 0; i < [[pageView content] count]; ++i) {
      AppsGridViewItem* gridItem = base::mac::ObjCCastStrict<AppsGridViewItem>(
          [pageView itemAtIndex:i]);
      [gridItem setModel:NULL];
    }
  }

  NSRange inPageRange = NSIntersectionRange(
      NSMakeRange(pageIndex * kItemsPerPage, kItemsPerPage),
      NSMakeRange(0, [items_ count]));
  NSArray* pageContent = [items_ subarrayWithRange:inPageRange];
  [pageView setContent:pageContent];
  if (!resetModel)
    return;

  for (size_t i = 0; i < [pageContent count]; ++i) {
    AppsGridViewItem* gridItem = base::mac::ObjCCastStrict<AppsGridViewItem>(
        [pageView itemAtIndex:i]);
    [gridItem setModel:static_cast<app_list::AppListItem*>(
        [[pageContent objectAtIndex:i] pointerValue])];
  }
}

- (void)moveItemInView:(size_t)fromIndex
           toItemIndex:(size_t)toIndex {
  base::scoped_nsobject<NSValue> item(
      [[items_ objectAtIndex:fromIndex] retain]);
  [items_ removeObjectAtIndex:fromIndex];
  [items_ insertObject:item
               atIndex:toIndex];

  size_t fromPageIndex = fromIndex / kItemsPerPage;
  size_t toPageIndex = toIndex / kItemsPerPage;
  if (fromPageIndex == toPageIndex) {
    [self updatePageContent:fromPageIndex
                 resetModel:NO];  // Just reorder items.
    return;
  }

  if (fromPageIndex > toPageIndex)
    std::swap(fromPageIndex, toPageIndex);

  for (size_t i = fromPageIndex; i <= toPageIndex; ++i) {
    [self updatePageContent:i
                 resetModel:YES];
  }
}

// Compare with views implementation in AppsGridView::MoveItemInModel().
- (void)moveItemWithIndex:(size_t)itemIndex
             toModelIndex:(size_t)modelIndex {
  // Ingore no-op moves. Note that this is always the case when canceled.
  if (itemIndex == modelIndex)
    return;

  app_list::AppListItemList* itemList = [self model]->top_level_item_list();
  itemList->RemoveObserver(bridge_.get());
  itemList->MoveItem(itemIndex, modelIndex);
  itemList->AddObserver(bridge_.get());
}

- (AppsCollectionViewDragManager*)dragManager {
  return dragManager_;
}

- (size_t)scheduledScrollPage {
  return scheduledScrollPage_;
}

- (void)listItemAdded:(size_t)index
                 item:(app_list::AppListItem*)itemModel {
  // Cancel any drag, to ensure the model stays consistent.
  [dragManager_ cancelDrag];

  [items_ insertObject:[NSValue valueWithPointer:itemModel]
              atIndex:index];

  [self updatePages:index];
}

- (void)listItemRemoved:(size_t)index {
  [dragManager_ cancelDrag];

  // Clear the models explicitly to avoid surprises from autorelease.
  [[self itemAtIndex:index] setModel:NULL];

  [items_ removeObjectsInRange:NSMakeRange(index, 1)];
  [self updatePages:index];
}

- (void)listItemMovedFromIndex:(size_t)fromIndex
                  toModelIndex:(size_t)toIndex {
  [dragManager_ cancelDrag];
  [self moveItemInView:fromIndex
           toItemIndex:toIndex];
}

- (CGFloat)visiblePortionOfPage:(int)page {
  CGFloat scrollOffsetOfPage =
      NSMinX([[[self gridScrollView] contentView] bounds]) / kViewWidth - page;
  if (scrollOffsetOfPage <= -1.0 || scrollOffsetOfPage >= 1.0)
    return 0.0;

  if (scrollOffsetOfPage <= 0.0)
    return scrollOffsetOfPage + 1.0;

  return -1.0 + scrollOffsetOfPage;
}

- (void)onPagerClicked:(AppListPagerView*)sender {
  int selectedSegment = [sender selectedSegment];
  if (selectedSegment < 0)
    return;  // No selection.

  int pageIndex = [[sender cell] tagForSegment:selectedSegment];
  if (pageIndex >= 0)
    [self scrollToPage:pageIndex];
}

- (BOOL)moveSelectionByDelta:(int)indexDelta {
  if (indexDelta == 0)
    return NO;

  NSUInteger oldIndex = [self selectedItemIndex];

  // If nothing is currently selected, select the first item on the page.
  if (oldIndex == NSNotFound) {
    [self selectItemAtIndex:visiblePage_ * kItemsPerPage];
    return YES;
  }

  // Can't select a negative index.
  if (indexDelta < 0 && static_cast<NSUInteger>(-indexDelta) > oldIndex)
    return NO;

  // Can't select an index greater or equal to the number of items.
  if (oldIndex + indexDelta >= [items_ count]) {
    if (visiblePage_ == [pages_ count] - 1)
      return NO;

    // If we're not on the last page, then select the last item.
    [self selectItemAtIndex:[items_ count] - 1];
    return YES;
  }

  [self selectItemAtIndex:oldIndex + indexDelta];
  return YES;
}

- (void)selectItemAtIndex:(NSUInteger)index {
  if (index >= [items_ count])
    return;

  if (index / kItemsPerPage != visiblePage_)
    [self scrollToPage:index / kItemsPerPage];

  [[self itemAtIndex:index] setSelected:YES];
}

- (BOOL)handleCommandBySelector:(SEL)command {
  if (command == @selector(insertNewline:) ||
      command == @selector(insertLineBreak:)) {
    [self activateSelection];
    return YES;
  }

  NSUInteger oldIndex = [self selectedItemIndex];
  // If nothing is currently selected, select the first item on the page.
  if (oldIndex == NSNotFound) {
    [self selectItemAtIndex:visiblePage_ * kItemsPerPage];
    return YES;
  }

  if (command == @selector(moveLeft:)) {
    return oldIndex % kFixedColumns == 0 ?
        [self moveSelectionByDelta:-kItemsPerPage + kFixedColumns - 1] :
        [self moveSelectionByDelta:-1];
  }

  if (command == @selector(moveRight:)) {
    return oldIndex % kFixedColumns == kFixedColumns - 1 ?
        [self moveSelectionByDelta:+kItemsPerPage - kFixedColumns + 1] :
        [self moveSelectionByDelta:1];
  }

  if (command == @selector(moveUp:)) {
    return oldIndex / kFixedColumns % kFixedRows == 0 ?
        NO : [self moveSelectionByDelta:-kFixedColumns];
  }

  if (command == @selector(moveDown:)) {
    return oldIndex / kFixedColumns % kFixedRows == kFixedRows - 1 ?
        NO : [self moveSelectionByDelta:kFixedColumns];
  }

  if (command == @selector(pageUp:) ||
      command == @selector(scrollPageUp:))
    return [self moveSelectionByDelta:-kItemsPerPage];

  if (command == @selector(pageDown:) ||
      command == @selector(scrollPageDown:))
    return [self moveSelectionByDelta:kItemsPerPage];

  return NO;
}

@end
