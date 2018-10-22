// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_controller.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/ui/material_components/chrome_app_bar_view_controller.h"
#import "ios/chrome/browser/ui/reading_list/empty_reading_list_background_view.h"
#import "ios/chrome/browser/ui/reading_list/legacy_reading_list_toolbar.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_data_sink.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_data_source.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_updater.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_view_controller_audience.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_view_controller_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Types of ListItems used by the reading list UI.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeItem,
};
// Identifiers for sections in the reading list.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierUnread = kSectionIdentifierEnumZero,
  SectionIdentifierRead,
};
}  // namespace

@interface ReadingListCollectionViewController ()<
    ReadingListDataSink,
    UIGestureRecognizerDelegate> {
  // Toolbar with the actions.
  LegacyReadingListToolbar* _toolbar;
  // Action sheet presenting the subactions of the toolbar.
  AlertCoordinator* _actionSheet;
  UIView* _emptyCollectionBackground;

  // Whether the data source has pending modifications.
  BOOL _dataSourceHasBeenModified;
}

// Redefine the model to return ReadingListListItems
@property(nonatomic, readonly)
    CollectionViewModel<CollectionViewItem<ReadingListListItem>*>*
        collectionViewModel;

// Whether the data source modifications should be taken into account.
@property(nonatomic, assign) BOOL shouldMonitorDataSource;

// Casts |item| to a CollectionViewItem.
- (CollectionViewItem<ReadingListListItem>*)collectionItemForReadingListItem:
    (id<ReadingListListItem>)item;
// Handles "Done" button touches.
- (void)donePressed;
// Loads all the items in all sections.
- (void)loadItems;
// Fills section |sectionIdentifier| with the items from |array|.
- (void)loadItemsFromArray:(NSArray<id<ReadingListListItem>>*)array
                 toSection:(SectionIdentifier)sectionIdentifier;
// Reloads the data if a change occurred during editing
- (void)applyPendingUpdates;
// Returns whether there are elements in the section identified by
// |sectionIdentifier|.
- (BOOL)hasItemInSection:(SectionIdentifier)sectionIdentifier;
// Adds an empty background if needed.
- (void)collectionIsEmpty;
// Handles a long press.
- (void)handleLongPress:(UILongPressGestureRecognizer*)gestureRecognizer;
// Updates the toolbar state according to the selected items.
- (void)updateToolbarState;
// Displays an action sheet to let the user choose to mark all the elements as
// read or as unread. Used when nothing is selected.
- (void)markAllItemsAs;
// Displays an action sheet to let the user choose to mark all the selected
// elements as read or as unread. Used if read and unread elements are selected.
- (void)markMixedItemsAs;
// Marks all items as read.
- (void)markAllRead;
// Marks all items as unread.
- (void)markAllUnread;
// Marks the items at |indexPaths| as read.
- (void)markItemsReadAtIndexPath:(NSArray*)indexPaths;
// Marks the items at |indexPaths| as unread.
- (void)markItemsUnreadAtIndexPath:(NSArray*)indexPaths;
// Deletes all the read items.
- (void)deleteAllReadItems;
// Deletes all the items at |indexPath|.
- (void)deleteItemsAtIndexPaths:(NSArray*)indexPaths;
// Initializes |_actionSheet| with |self| as base view controller, and the
// toolbar's mark button as anchor point.
- (void)initializeActionSheet;
// Exits the editing mode and update the toolbar state with animation.
- (void)exitEditingModeAnimated:(BOOL)animated;
// Applies |updater| to the URL of every cell in the section |identifier|. The
// updates are done in reverse order of the cells in the section to keep the
// order. The monitoring of the data source updates are suspended during this
// time.
- (void)updateItemsInSectionIdentifier:(SectionIdentifier)identifier
                      usingItemUpdater:(ReadingListListItemUpdater)updater;
// Applies |updater| to the URL of every element in |indexPaths|. The updates
// are done in reverse order |indexPaths| to keep the order. The monitoring of
// the data source updates are suspended during this time.
- (void)updateIndexPaths:(NSArray<NSIndexPath*>*)indexPaths
        usingItemUpdater:(ReadingListListItemUpdater)updater;
// Move all the items from |sourceSectionIdentifier| to
// |destinationSectionIdentifier| and removes the empty section from the
// collection.
- (void)moveItemsFromSection:(SectionIdentifier)sourceSectionIdentifier
                   toSection:(SectionIdentifier)destinationSectionIdentifier;
// Move the currently selected elements to |sectionIdentifier| and removes the
// empty sections.
- (void)moveSelectedItems:(NSArray*)sortedIndexPaths
                toSection:(SectionIdentifier)sectionIdentifier;
// Makes sure |sectionIdentifier| exists with the correct header.
// Returns the index of the new section in the collection view; NSIntegerMax if
// no section has been created.
- (NSInteger)initializeSection:(SectionIdentifier)sectionIdentifier;
// Returns the header for the |sectionIdentifier|.
- (CollectionViewTextItem*)headerForSection:
    (SectionIdentifier)sectionIdentifier;
// Removes the empty sections from the collection and the model.
- (void)removeEmptySections;

@end

@implementation ReadingListCollectionViewController

@synthesize audience = _audience;
@synthesize delegate = _delegate;
@synthesize dataSource = _dataSource;

@dynamic collectionViewModel;
@synthesize shouldMonitorDataSource = _shouldMonitorDataSource;

+ (NSString*)accessibilityIdentifier {
  return @"ReadingListCollectionView";
}

#pragma mark lifecycle

- (instancetype)initWithDataSource:(id<ReadingListDataSource>)dataSource
                           toolbar:(LegacyReadingListToolbar*)toolbar {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    _toolbar = toolbar;

    _dataSource = dataSource;
    _emptyCollectionBackground = [[EmptyReadingListBackgroundView alloc] init];

    _shouldMonitorDataSource = YES;
    _dataSourceHasBeenModified = NO;

    _dataSource.dataSink = self;
  }
  return self;
}

#pragma mark - properties

- (void)setToolbarState:(LegacyReadingListToolbarState)toolbarState {
  [_toolbar setState:toolbarState];
}

- (void)setAudience:(id<ReadingListListViewControllerAudience>)audience {
  _audience = audience;
  if (self.dataSource.ready) {
    [audience readingListHasItems:self.dataSource.hasElements];
  }
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_READING_LIST);
  self.collectionView.accessibilityIdentifier =
      [ReadingListCollectionViewController accessibilityIdentifier];
  // Add "Done" button.
  UIBarButtonItem* doneItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_READING_LIST_DONE_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(donePressed)];
  doneItem.accessibilityIdentifier = @"Done";
  self.navigationItem.rightBarButtonItem = doneItem;

  // Customize collection view settings.
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  self.styler.separatorInset = UIEdgeInsetsMake(0, 16, 0, 16);

  UILongPressGestureRecognizer* longPressRecognizer =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleLongPress:)];
  longPressRecognizer.delegate = self;
  [self.collectionView addGestureRecognizer:longPressRecognizer];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [_toolbar updateHeight];
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  if (self.editor.editing) {
    [self updateToolbarState];
  } else {
    id<ReadingListListItem> item =
        [self.collectionViewModel itemAtIndexPath:indexPath];
    [self.delegate readingListListViewController:self openItem:item];
  }
}

- (void)collectionView:(UICollectionView*)collectionView
    didDeselectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didDeselectItemAtIndexPath:indexPath];
  if (self.editor.editing) {
    // When deselecting an item, if we are editing, we want to update the
    // toolbar based on the selected items.
    [self updateToolbarState];
  }
}

#pragma mark - MDCCollectionViewController

- (void)updateFooterInfoBarIfNecessary {
  // No-op. This prevents the default infobar from showing.
  // TODO(crbug.com/653547): Remove this once the MDC adds an option for
  // preventing the infobar from showing.
}

#pragma mark - MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger type = [self.collectionViewModel itemTypeForIndexPath:indexPath];
  if (type == ItemTypeItem)
    return MDCCellDefaultTwoLineHeight;
  else
    return MDCCellDefaultOneLineHeight;
}

#pragma mark - MDCCollectionViewEditingDelegate

- (BOOL)collectionViewAllowsEditing:(nonnull UICollectionView*)collectionView {
  return YES;
}

#pragma mark - ReadingListDataSink

- (void)dataSourceReady:(id<ReadingListDataSource>)dataSource {
  [self loadModel];
  if ([self isViewLoaded]) {
    [self.collectionView reloadData];
  }
}

- (void)dataSourceChanged {
  // If we are editing and monitoring the model updates, set a flag to reload
  // the data at the end of the editing.
  if (self.editor.editing) {
    _dataSourceHasBeenModified = YES;
  } else {
    [self reloadData];
  }
}

- (NSArray<id<ReadingListListItem>>*)readItems {
  return [self readingListItemsForSection:SectionIdentifierRead];
}

- (NSArray<id<ReadingListListItem>>*)unreadItems {
  return [self readingListItemsForSection:SectionIdentifierUnread];
}

- (void)itemHasChangedAfterDelay:(id<ReadingListListItem>)item {
  CollectionViewItem<ReadingListListItem>* collectionItem =
      [self collectionItemForReadingListItem:item];
  if ([self.collectionViewModel hasItem:collectionItem]) {
    [self reconfigureCellsForItems:@[ collectionItem ]];
  }
}

- (void)itemsHaveChanged:(NSArray<id<ReadingListListItem>>*)items {
  [self reconfigureCellsForItems:items];
}

#pragma mark - ReadingListDataSink Helpers

- (NSArray<id<ReadingListListItem>>*)readingListItemsForSection:
    (SectionIdentifier)sectionID {
  if (![self.collectionViewModel hasSectionForSectionIdentifier:sectionID]) {
    return nil;
  }
  NSMutableArray<id<ReadingListListItem>>* items = [NSMutableArray array];
  NSArray<CollectionViewItem*>* sectionItems =
      [self.collectionViewModel itemsInSectionWithIdentifier:sectionID];
  for (id<ReadingListListItem> item in sectionItems) {
    [items addObject:item];
  }
  return items;
}

#pragma mark - Public methods

- (void)reloadData {
  [self loadModel];
  if ([self isViewLoaded]) {
    [self.collectionView reloadData];
  }
}

- (void)willBeDismissed {
  [self.dataSource dataSinkWillBeDismissed];
  [_actionSheet stop];
}

#pragma mark - ReadingListListItemAccessibilityDelegate

- (BOOL)isItemRead:(id<ReadingListListItem>)item {
  return [self.dataSource isItemRead:item];
}

- (void)deleteItem:(id<ReadingListListItem>)item {
  CollectionViewItem<ReadingListListItem>* collectionItem =
      [self collectionItemForReadingListItem:item];
  if ([self.collectionViewModel hasItem:collectionItem]) {
    [self deleteItemsAtIndexPaths:@[ [self.collectionViewModel
                                      indexPathForItem:collectionItem] ]];
  }
}

- (void)openItemInNewTab:(id<ReadingListListItem>)item {
  [self.delegate readingListListViewController:self
                              openItemInNewTab:item
                                     incognito:NO];
}

- (void)openItemInNewIncognitoTab:(id<ReadingListListItem>)item {
  [self.delegate readingListListViewController:self
                              openItemInNewTab:item
                                     incognito:YES];
}

- (void)openItemOffline:(id<ReadingListListItem>)item {
  [self.delegate readingListListViewController:self
                       openItemOfflineInNewTab:item];
}

- (void)markItemRead:(id<ReadingListListItem>)item {
  CollectionViewItem<ReadingListListItem>* collectionItem =
      [self collectionItemForReadingListItem:item];
  if ([self.collectionViewModel hasItem:collectionItem
                inSectionWithIdentifier:SectionIdentifierUnread]) {
    [self markItemsReadAtIndexPath:@[ [self.collectionViewModel
                                       indexPathForItem:collectionItem] ]];
  }
}

- (void)markItemUnread:(id<ReadingListListItem>)item {
  CollectionViewItem<ReadingListListItem>* collectionItem =
      [self collectionItemForReadingListItem:item];
  if ([self.collectionViewModel hasItem:collectionItem
                inSectionWithIdentifier:SectionIdentifierRead]) {
    [self markItemsUnreadAtIndexPath:@[ [self.collectionViewModel
                                         indexPathForItem:collectionItem] ]];
  }
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  // Prevent the context menu to be displayed if the long press is done on the
  // App Bar.
  CGPoint location =
      [touch locationInView:self.appBarViewController.navigationBar];
  return !CGRectContainsPoint(self.appBarViewController.navigationBar.frame,
                              location);
}

#pragma mark - Private methods

- (void)donePressed {
  if ([self.editor isEditing]) {
    [self exitEditingModeAnimated:NO];
  }
  [self dismiss];
}

- (void)dismiss {
  [self.delegate dismissReadingListListViewController:self];
}

- (void)loadModel {
  [super loadModel];
  _dataSourceHasBeenModified = NO;

  if (!self.dataSource.hasElements) {
    [self collectionIsEmpty];
  } else {
    self.collectionView.alwaysBounceVertical = YES;
    [self loadItems];
    self.collectionView.backgroundView = nil;
    [self.audience readingListHasItems:YES];
  }
}

- (void)loadItemsFromArray:(NSArray<id<ReadingListListItem>>*)items
                 toSection:(SectionIdentifier)sectionIdentifier {
  if (items.count == 0) {
    return;
  }
  CollectionViewModel* model = self.collectionViewModel;
  [model addSectionWithIdentifier:sectionIdentifier];
  [model setHeader:[self headerForSection:sectionIdentifier]
      forSectionWithIdentifier:sectionIdentifier];
  for (CollectionViewItem<ReadingListListItem>* item in items) {
    item.type = ItemTypeItem;
    [self.dataSource fetchFaviconForItem:item];
    [model addItem:item toSectionWithIdentifier:sectionIdentifier];
  }
}

- (void)loadItems {
  NSMutableArray<id<ReadingListListItem>>* readArray = [NSMutableArray array];
  NSMutableArray<id<ReadingListListItem>>* unreadArray = [NSMutableArray array];
  [self.dataSource fillReadItems:readArray unreadItems:unreadArray];
  [self loadItemsFromArray:unreadArray toSection:SectionIdentifierUnread];
  [self loadItemsFromArray:readArray toSection:SectionIdentifierRead];

  BOOL hasRead = readArray.count > 0;
  [_toolbar setHasReadItem:hasRead];
}

- (void)applyPendingUpdates {
  if (_dataSourceHasBeenModified) {
    [self reloadData];
  }
}

- (BOOL)hasItemInSection:(SectionIdentifier)sectionIdentifier {
  if (![self.collectionViewModel
          hasSectionForSectionIdentifier:sectionIdentifier]) {
    // No section.
    return NO;
  }

  NSInteger section =
      [self.collectionViewModel sectionForSectionIdentifier:sectionIdentifier];
  NSInteger numberOfItems =
      [self.collectionViewModel numberOfItemsInSection:section];

  return numberOfItems > 0;
}

- (void)collectionIsEmpty {
  if (self.collectionView.backgroundView) {
    return;
  }
  // The collection is empty, add background.
  self.collectionView.alwaysBounceVertical = NO;
  self.collectionView.backgroundView = _emptyCollectionBackground;
  [self.audience readingListHasItems:NO];
}

- (void)handleLongPress:(UILongPressGestureRecognizer*)gestureRecognizer {
  if (self.editor.editing ||
      gestureRecognizer.state != UIGestureRecognizerStateBegan) {
    return;
  }

  CGPoint touchLocation =
      [gestureRecognizer locationOfTouch:0 inView:self.collectionView];
  NSIndexPath* touchedItemIndexPath =
      [self.collectionView indexPathForItemAtPoint:touchLocation];
  if (!touchedItemIndexPath ||
      ![self.collectionViewModel hasItemAtIndexPath:touchedItemIndexPath]) {
    // Make sure there is an item at this position.
    return;
  }

  CollectionViewItem<ReadingListListItem>* item =
      [self.collectionViewModel itemAtIndexPath:touchedItemIndexPath];
  if (item.type != ItemTypeItem) {
    // Do not trigger context menu on headers.
    return;
  }

  [self.delegate readingListListViewController:self
                     displayContextMenuForItem:item
                                       atPoint:touchLocation];
}

#pragma mark - LegacyReadingListToolbarDelegate

- (void)markPressed {
  if (![self.editor isEditing]) {
    return;
  }
  switch ([_toolbar state]) {
    case NoneSelected:
      [self markAllItemsAs];
      break;
    case OnlyUnreadSelected:
      [self markItemsReadAtIndexPath:self.collectionView
                                         .indexPathsForSelectedItems];
      break;
    case OnlyReadSelected:
      [self markItemsUnreadAtIndexPath:self.collectionView
                                           .indexPathsForSelectedItems];
      break;
    case MixedItemsSelected:
      [self markMixedItemsAs];
      break;
  }
}

- (void)deletePressed {
  if (![self.editor isEditing]) {
    return;
  }
  if ([_toolbar state] == NoneSelected) {
    [self deleteAllReadItems];
  } else {
    [self
        deleteItemsAtIndexPaths:self.collectionView.indexPathsForSelectedItems];
  }
}
- (void)enterEditingModePressed {
  if ([self.editor isEditing]) {
    return;
  }
  self.toolbarState = NoneSelected;
  [self.editor setEditing:YES animated:YES];
  [_toolbar setEditing:YES];
}

- (void)exitEditingModePressed {
  if (![self.editor isEditing]) {
    return;
  }
  [self exitEditingModeAnimated:YES];
}

#pragma mark - Private methods - Toolbar

- (CollectionViewItem<ReadingListListItem>*)collectionItemForReadingListItem:
    (id<ReadingListListItem>)item {
  return base::mac::ObjCCastStrict<CollectionViewItem<ReadingListListItem>>(
      item);
}

- (void)updateToolbarState {
  BOOL readSelected = NO;
  BOOL unreadSelected = NO;

  if ([self.collectionView.indexPathsForSelectedItems count] == 0) {
    // No entry selected.
    self.toolbarState = NoneSelected;
    return;
  }

  // Sections for section identifiers.
  NSInteger sectionRead = NSNotFound;
  NSInteger sectionUnread = NSNotFound;

  if ([self hasItemInSection:SectionIdentifierRead]) {
    sectionRead = [self.collectionViewModel
        sectionForSectionIdentifier:SectionIdentifierRead];
  }
  if ([self hasItemInSection:SectionIdentifierUnread]) {
    sectionUnread = [self.collectionViewModel
        sectionForSectionIdentifier:SectionIdentifierUnread];
  }

  // Check selected sections.
  for (NSIndexPath* index in self.collectionView.indexPathsForSelectedItems) {
    if (index.section == sectionRead) {
      readSelected = YES;
    } else if (index.section == sectionUnread) {
      unreadSelected = YES;
    }
  }

  // Update toolbar state.
  if (readSelected) {
    if (unreadSelected) {
      // Read and Unread selected.
      self.toolbarState = MixedItemsSelected;
    } else {
      // Read selected.
      self.toolbarState = OnlyReadSelected;
    }
  } else if (unreadSelected) {
    // Unread selected.
    self.toolbarState = OnlyUnreadSelected;
  }
}

- (void)markAllItemsAs {
  [self initializeActionSheet];
  __weak ReadingListCollectionViewController* weakSelf = self;
  [_actionSheet addItemWithTitle:l10n_util::GetNSStringWithFixup(
                                     IDS_IOS_READING_LIST_MARK_ALL_READ_ACTION)
                          action:^{
                            [weakSelf markAllRead];
                          }
                           style:UIAlertActionStyleDefault];
  [_actionSheet
      addItemWithTitle:l10n_util::GetNSStringWithFixup(
                           IDS_IOS_READING_LIST_MARK_ALL_UNREAD_ACTION)
                action:^{
                  [weakSelf markAllUnread];
                }
                 style:UIAlertActionStyleDefault];
  [_actionSheet start];
}

- (void)markMixedItemsAs {
  [self initializeActionSheet];
  __weak ReadingListCollectionViewController* weakSelf = self;
  [_actionSheet
      addItemWithTitle:l10n_util::GetNSStringWithFixup(
                           IDS_IOS_READING_LIST_MARK_READ_BUTTON)
                action:^{
                  [weakSelf
                      markItemsReadAtIndexPath:weakSelf.collectionView
                                                   .indexPathsForSelectedItems];
                }
                 style:UIAlertActionStyleDefault];
  [_actionSheet
      addItemWithTitle:l10n_util::GetNSStringWithFixup(
                           IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON)
                action:^{
                  [weakSelf
                      markItemsUnreadAtIndexPath:
                          weakSelf.collectionView.indexPathsForSelectedItems];
                }
                 style:UIAlertActionStyleDefault];
  [_actionSheet start];
}

- (void)initializeActionSheet {
  _actionSheet = [_toolbar actionSheetForMarkWithBaseViewController:self];

  [_actionSheet addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_CANCEL)
                          action:nil
                           style:UIAlertActionStyleCancel];
}

- (void)markAllRead {
  if (![self.editor isEditing]) {
    return;
  }
  if (![self hasItemInSection:SectionIdentifierUnread]) {
    [self exitEditingModeAnimated:YES];
    return;
  }

  [self updateItemsInSectionIdentifier:SectionIdentifierUnread
                      usingItemUpdater:^(id<ReadingListListItem> item) {
                        [self.dataSource setReadStatus:YES forItem:item];
                      }];

  [self exitEditingModeAnimated:YES];
  [self moveItemsFromSection:SectionIdentifierUnread
                   toSection:SectionIdentifierRead];
}

- (void)markAllUnread {
  if (![self hasItemInSection:SectionIdentifierRead]) {
    [self exitEditingModeAnimated:YES];
    return;
  }

  [self updateItemsInSectionIdentifier:SectionIdentifierRead
                      usingItemUpdater:^(id<ReadingListListItem> item) {
                        [self.dataSource setReadStatus:NO forItem:item];
                      }];

  [self exitEditingModeAnimated:YES];
  [self moveItemsFromSection:SectionIdentifierRead
                   toSection:SectionIdentifierUnread];
}

- (void)markItemsReadAtIndexPath:(NSArray*)indexPaths {
  base::RecordAction(base::UserMetricsAction("MobileReadingListMarkRead"));
  NSArray* sortedIndexPaths =
      [indexPaths sortedArrayUsingSelector:@selector(compare:)];
  [self updateIndexPaths:sortedIndexPaths
        usingItemUpdater:^(id<ReadingListListItem> item) {
          [self.dataSource setReadStatus:YES forItem:item];
        }];

  [self exitEditingModeAnimated:YES];
  [self moveSelectedItems:sortedIndexPaths toSection:SectionIdentifierRead];
}

- (void)markItemsUnreadAtIndexPath:(NSArray*)indexPaths {
  base::RecordAction(base::UserMetricsAction("MobileReadingListMarkUnread"));
  NSArray* sortedIndexPaths =
      [indexPaths sortedArrayUsingSelector:@selector(compare:)];
  [self updateIndexPaths:sortedIndexPaths
        usingItemUpdater:^(id<ReadingListListItem> item) {
          [self.dataSource setReadStatus:NO forItem:item];
        }];

  [self exitEditingModeAnimated:YES];
  [self moveSelectedItems:sortedIndexPaths toSection:SectionIdentifierUnread];
}

- (void)deleteAllReadItems {
  base::RecordAction(base::UserMetricsAction("MobileReadingListDeleteRead"));
  if (![self hasItemInSection:SectionIdentifierRead]) {
    [self exitEditingModeAnimated:YES];
    return;
  }

  [self updateItemsInSectionIdentifier:SectionIdentifierRead
                      usingItemUpdater:^(id<ReadingListListItem> item) {
                        [self.dataSource removeEntryFromItem:item];
                      }];

  [self exitEditingModeAnimated:YES];
  [self.collectionView performBatchUpdates:^{
    NSInteger readSection = [self.collectionViewModel
        sectionForSectionIdentifier:SectionIdentifierRead];
    [self.collectionView
        deleteSections:[NSIndexSet indexSetWithIndex:readSection]];
    [self.collectionViewModel
        removeSectionWithIdentifier:SectionIdentifierRead];
  }
      completion:^(BOOL) {
        // Reload data to take into account possible sync events.
        [self applyPendingUpdates];
      }];
  // As we modified the section in the batch update block, remove the section in
  // another block.
  [self removeEmptySections];
}

- (void)deleteItemsAtIndexPaths:(NSArray*)indexPaths {
  [self updateIndexPaths:indexPaths
        usingItemUpdater:^(id<ReadingListListItem> item) {
          [self.dataSource removeEntryFromItem:item];
        }];

  [self exitEditingModeAnimated:YES];

  [self.collectionView performBatchUpdates:^{
    [self collectionView:self.collectionView
        willDeleteItemsAtIndexPaths:indexPaths];

    [self.collectionView deleteItemsAtIndexPaths:indexPaths];
  }
      completion:^(BOOL) {
        // Reload data to take into account possible sync events.
        [self applyPendingUpdates];
      }];
  // As we modified the section in the batch update block, remove the section in
  // another block.
  [self removeEmptySections];
}

- (void)updateItemsInSectionIdentifier:(SectionIdentifier)identifier
                      usingItemUpdater:(ReadingListListItemUpdater)updater {
  [self.dataSource beginBatchUpdates];
  NSArray* readItems =
      [self.collectionViewModel itemsInSectionWithIdentifier:identifier];
  // Read the objects in reverse order to keep the order (last modified first).
  for (id item in [readItems reverseObjectEnumerator]) {
    if (updater)
      updater(item);
  }
  [self.dataSource endBatchUpdates];
}

- (void)updateIndexPaths:(NSArray<NSIndexPath*>*)indexPaths
        usingItemUpdater:(ReadingListListItemUpdater)updater {
  [self.dataSource beginBatchUpdates];
  // Read the objects in reverse order to keep the order (last modified first).
  for (NSIndexPath* index in [indexPaths reverseObjectEnumerator]) {
    id<ReadingListListItem> item =
        [self.collectionViewModel itemAtIndexPath:index];
    if (updater)
      updater(item);
  }
  [self.dataSource endBatchUpdates];
}

- (void)moveItemsFromSection:(SectionIdentifier)sourceSectionIdentifier
                   toSection:(SectionIdentifier)destinationSectionIdentifier {
  NSInteger sourceSection = [self.collectionViewModel
      sectionForSectionIdentifier:sourceSectionIdentifier];
  NSInteger numberOfSourceItems =
      [self.collectionViewModel numberOfItemsInSection:sourceSection];

  NSMutableArray* sortedIndexPaths = [NSMutableArray array];

  for (int index = 0; index < numberOfSourceItems; index++) {
    NSIndexPath* itemIndex =
        [NSIndexPath indexPathForItem:index inSection:sourceSection];
    [sortedIndexPaths addObject:itemIndex];
  }

  [self moveSelectedItems:sortedIndexPaths
                toSection:destinationSectionIdentifier];
}

- (void)moveSelectedItems:(NSArray*)sortedIndexPaths
                toSection:(SectionIdentifier)sectionIdentifier {
  // Reconfigure cells, allowing the custom actions to be updated.
  [self reconfigureCellsAtIndexPaths:sortedIndexPaths];

  NSInteger sectionCreatedIndex = [self initializeSection:sectionIdentifier];

  [self.collectionView performBatchUpdates:^{
    NSInteger section = [self.collectionViewModel
        sectionForSectionIdentifier:sectionIdentifier];

    NSInteger newItemIndex = 0;
    for (NSIndexPath* index in sortedIndexPaths) {
      // The |sortedIndexPaths| is a copy of the index paths before the
      // destination section has been added if necessary. The section part of
      // the index potentially needs to be updated.
      NSInteger updatedSection = index.section;
      if (updatedSection >= sectionCreatedIndex)
        updatedSection++;
      if (updatedSection == section) {
        // The item is already in the targeted section, there is no need to move
        // it.
        continue;
      }

      NSIndexPath* updatedIndex =
          [NSIndexPath indexPathForItem:index.item inSection:updatedSection];
      NSIndexPath* indexForModel =
          [NSIndexPath indexPathForItem:index.item - newItemIndex
                              inSection:updatedSection];

      // Index of the item in the new section. The newItemIndex is the index of
      // this item in the targeted section.
      NSIndexPath* newIndexPath =
          [NSIndexPath indexPathForItem:newItemIndex++ inSection:section];

      [self collectionView:self.collectionView
          willMoveItemAtIndexPath:indexForModel
                      toIndexPath:newIndexPath];
      [self.collectionView moveItemAtIndexPath:updatedIndex
                                   toIndexPath:newIndexPath];
    }
  }
      completion:^(BOOL) {
        // Reload data to take into account possible sync events.
        [self applyPendingUpdates];
      }];
  // As we modified the section in the batch update block, remove the section in
  // another block.
  [self removeEmptySections];
}

- (NSInteger)initializeSection:(SectionIdentifier)sectionIdentifier {
  if (![self.collectionViewModel
          hasSectionForSectionIdentifier:sectionIdentifier]) {
    // The new section IndexPath will be 1 if it is the read section with
    // items in the unread section, 0 otherwise.
    BOOL hasNonEmptySectionAbove =
        sectionIdentifier == SectionIdentifierRead &&
        [self hasItemInSection:SectionIdentifierUnread];
    NSInteger sectionIndex = hasNonEmptySectionAbove ? 1 : 0;

    [self.collectionView performBatchUpdates:^{
      [self.collectionViewModel insertSectionWithIdentifier:sectionIdentifier
                                                    atIndex:sectionIndex];

      [self.collectionViewModel
                         setHeader:[self headerForSection:sectionIdentifier]
          forSectionWithIdentifier:sectionIdentifier];

      [self.collectionView
          insertSections:[NSIndexSet indexSetWithIndex:sectionIndex]];
    }
                                  completion:nil];

    return sectionIndex;
  }
  return NSIntegerMax;
}

- (CollectionViewTextItem*)headerForSection:
    (SectionIdentifier)sectionIdentifier {
  CollectionViewTextItem* header =
      [[CollectionViewTextItem alloc] initWithType:ItemTypeHeader];

  switch (sectionIdentifier) {
    case SectionIdentifierRead:
      header.text = l10n_util::GetNSString(IDS_IOS_READING_LIST_READ_HEADER);
      break;
    case SectionIdentifierUnread:
      header.text = l10n_util::GetNSString(IDS_IOS_READING_LIST_UNREAD_HEADER);
      break;
  }
  header.textColor = [[MDCPalette greyPalette] tint500];
  return header;
}

- (void)removeEmptySections {
  [self.collectionView performBatchUpdates:^{

    SectionIdentifier a[] = {SectionIdentifierRead, SectionIdentifierUnread};
    for (size_t i = 0; i < arraysize(a); i++) {
      SectionIdentifier sectionIdentifier = a[i];

      if ([self.collectionViewModel
              hasSectionForSectionIdentifier:sectionIdentifier] &&
          ![self hasItemInSection:sectionIdentifier]) {
        NSInteger section = [self.collectionViewModel
            sectionForSectionIdentifier:sectionIdentifier];

        [self.collectionView
            deleteSections:[NSIndexSet indexSetWithIndex:section]];
        [self.collectionViewModel
            removeSectionWithIdentifier:sectionIdentifier];
      }
    }
  }
                                completion:nil];
  if (!self.dataSource.hasElements) {
    [self collectionIsEmpty];
  } else {
    [_toolbar setHasReadItem:self.dataSource.hasReadElements];
  }
}

- (void)exitEditingModeAnimated:(BOOL)animated {
  [_actionSheet stop];
  [self.editor setEditing:NO animated:animated];
  [_toolbar setEditing:NO];
}

@end
