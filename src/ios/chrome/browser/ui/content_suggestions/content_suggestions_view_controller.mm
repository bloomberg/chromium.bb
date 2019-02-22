// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_updater.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizing.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_layout.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recording.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/web/public/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using CSCollectionViewItem = CollectionViewItem<SuggestedContent>;
const CGFloat kMostVisitedBottomMargin = 13;
const CGFloat kCardBorderRadius = 11;

}

@interface ContentSuggestionsViewController ()<UIGestureRecognizerDelegate>

@property(nonatomic, strong)
    ContentSuggestionsCollectionUpdater* collectionUpdater;

// The overscroll actions controller managing accelerators over the toolbar.
@property(nonatomic, strong)
    OverscrollActionsController* overscrollActionsController;
@end

@implementation ContentSuggestionsViewController

@synthesize audience = _audience;
@synthesize suggestionCommandHandler = _suggestionCommandHandler;
@synthesize headerSynchronizer = _headerSynchronizer;
@synthesize collectionUpdater = _collectionUpdater;
@synthesize overscrollActionsController = _overscrollActionsController;
@synthesize overscrollDelegate = _overscrollDelegate;
@synthesize scrolledToTop = _scrolledToTop;
@synthesize metricsRecorder = _metricsRecorder;
@synthesize containsToolbar = _containsToolbar;
@dynamic collectionViewModel;

#pragma mark - Lifecycle

- (instancetype)initWithStyle:(CollectionViewControllerStyle)style {
  UICollectionViewLayout* layout = [[ContentSuggestionsLayout alloc] init];
  self = [super initWithLayout:layout style:style];
  if (self) {
    _collectionUpdater = [[ContentSuggestionsCollectionUpdater alloc] init];
  }
  return self;
}

- (void)dealloc {
  [self.overscrollActionsController invalidate];
}

#pragma mark - Public

- (void)setDataSource:(id<ContentSuggestionsDataSource>)dataSource {
  self.collectionUpdater.dataSource = dataSource;
}

- (void)setDispatcher:(id<SnackbarCommands>)dispatcher {
  self.collectionUpdater.dispatcher = dispatcher;
}

- (void)dismissEntryAtIndexPath:(NSIndexPath*)indexPath {
  if (!indexPath || ![self.collectionViewModel hasItemAtIndexPath:indexPath]) {
    return;
  }

  [self.metricsRecorder
      onSuggestionDismissed:[self.collectionViewModel itemAtIndexPath:indexPath]
                atIndexPath:indexPath
      suggestionsShownAbove:[self numberOfSuggestionsAbove:indexPath.section]];

  [self.collectionView performBatchUpdates:^{
    [self collectionView:self.collectionView
        willDeleteItemsAtIndexPaths:@[ indexPath ]];
    [self.collectionView deleteItemsAtIndexPaths:@[ indexPath ]];

    // Check if the section is now empty.
    [self addEmptySectionPlaceholderIfNeeded:indexPath.section];
  }
      completion:^(BOOL) {
        // The context menu could be displayed for the deleted entry.
        [self.suggestionCommandHandler dismissModals];
      }];
}

- (void)dismissSection:(NSInteger)section {
  if (section >= [self numberOfSectionsInCollectionView:self.collectionView]) {
    return;
  }

  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:section];

  [self.collectionView performBatchUpdates:^{
    [self.collectionViewModel removeSectionWithIdentifier:sectionIdentifier];
    [self.collectionView deleteSections:[NSIndexSet indexSetWithIndex:section]];
  }
      completion:^(BOOL) {
        // The context menu could be displayed for the deleted entries.
        [self.suggestionCommandHandler dismissModals];
      }];
}

- (void)addSuggestions:(NSArray<CSCollectionViewItem*>*)suggestions
         toSectionInfo:(ContentSuggestionsSectionInformation*)sectionInfo {
  void (^batchUpdates)(void) = ^{
    NSIndexSet* addedSections = [self.collectionUpdater
        addSectionsForSectionInfoToModel:@[ sectionInfo ]];
    [self.collectionView insertSections:addedSections];

    NSIndexPath* removedItem = [self.collectionUpdater
        removeEmptySuggestionsForSectionInfo:sectionInfo];
    if (removedItem) {
      [self.collectionView deleteItemsAtIndexPaths:@[ removedItem ]];
    }

    NSArray<NSIndexPath*>* addedItems =
        [self.collectionUpdater addSuggestionsToModel:suggestions
                                      withSectionInfo:sectionInfo];
    [self.collectionView insertItemsAtIndexPaths:addedItems];
  };

  [self.collectionView performBatchUpdates:batchUpdates completion:nil];
}

- (NSInteger)numberOfSuggestionsAbove:(NSInteger)section {
  NSInteger suggestionsAbove = 0;
  for (NSInteger sectionAbove = 0; sectionAbove < section; sectionAbove++) {
    if ([self.collectionUpdater isContentSuggestionsSection:sectionAbove]) {
      suggestionsAbove +=
          [self.collectionViewModel numberOfItemsInSection:sectionAbove];
    }
  }
  return suggestionsAbove;
}

- (NSInteger)numberOfSectionsAbove:(NSInteger)section {
  NSInteger sectionsAbove = 0;
  for (NSInteger sectionAbove = 0; sectionAbove < section; sectionAbove++) {
    if ([self.collectionUpdater isContentSuggestionsSection:sectionAbove]) {
      sectionsAbove++;
    }
  }
  return sectionsAbove;
}

- (void)updateConstraints {
  [self.collectionUpdater
      updateMostVisitedForSize:self.collectionView.bounds.size];
  [self.headerSynchronizer
      updateFakeOmniboxOnNewWidth:self.collectionView.bounds.size.width];
  [self.headerSynchronizer updateConstraints];
  [self.collectionView reloadData];
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
}

- (void)clearOverscroll {
  [self.overscrollActionsController clear];
}

+ (NSString*)collectionAccessibilityIdentifier {
  return @"ContentSuggestionsCollectionIdentifier";
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.collectionView.prefetchingEnabled = NO;
  if (@available(iOS 11, *)) {
    // Overscroll action does not work well with content offset, so set this
    // to never and internally offset the UI to account for safe area insets.
    self.collectionView.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;
  }
  self.collectionView.accessibilityIdentifier =
      [[self class] collectionAccessibilityIdentifier];
  _collectionUpdater.collectionViewController = self;

  self.collectionView.delegate = self;
  self.collectionView.backgroundColor = ntp_home::kNTPBackgroundColor();
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
  self.styler.cardBorderRadius = kCardBorderRadius;
  self.automaticallyAdjustsScrollViewInsets = NO;
  self.collectionView.translatesAutoresizingMaskIntoConstraints = NO;

  ApplyVisualConstraints(@[ @"V:|[collection]|", @"H:|[collection]|" ],
                         @{@"collection" : self.collectionView});

  UILongPressGestureRecognizer* longPressRecognizer =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleLongPress:)];
  longPressRecognizer.delegate = self;
  [self.collectionView addGestureRecognizer:longPressRecognizer];

  self.overscrollActionsController = [[OverscrollActionsController alloc]
      initWithScrollView:self.collectionView];
  [self.overscrollActionsController
      setStyle:OverscrollStyle::NTP_NON_INCOGNITO];
  self.overscrollActionsController.delegate = self.overscrollDelegate;
  [self updateOverscrollActionsState];
}

- (void)updateOverscrollActionsState {
  if (IsSplitToolbarMode(self)) {
    [self.overscrollActionsController enableOverscrollActions];
  } else {
    [self.overscrollActionsController disableOverscrollActions];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Reload data to ensure the Most Visited tiles and fakeOmnibox are correctly
  // positionned, in particular during a rotation while a ViewController is
  // presented in front of the NTP.
  [self.headerSynchronizer
      updateFakeOmniboxOnNewWidth:self.collectionView.bounds.size.width];
  [self.collectionView.collectionViewLayout invalidateLayout];
  // Ensure initial fake omnibox layout.
  [self.headerSynchronizer updateFakeOmniboxOnCollectionScroll];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  // Resize the collection as it might have been rotated while not being
  // presented (e.g. rotation on stack view).
  [self correctMissingSafeArea];
  [self updateConstraints];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (CGSizeEqualToSize(self.collectionView.bounds.size, CGSizeZero) &&
      !CGSizeEqualToSize(self.view.bounds.size, CGSizeZero)) {
    // When started after a cold start, the frame of the collection view isn't
    // set to the bounds of the view. In that case, the constraints for the
    // cells are broken.
    self.collectionView.frame = self.view.bounds;
  }
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [self.collectionUpdater updateMostVisitedForSize:size];
  [self.collectionView reloadData];

  void (^alongsideBlock)(id<UIViewControllerTransitionCoordinatorContext>) =
      ^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [self.headerSynchronizer updateFakeOmniboxOnNewWidth:size.width];
        [self.collectionView.collectionViewLayout invalidateLayout];
      };
  [coordinator animateAlongsideTransition:alongsideBlock completion:nil];
}

- (void)willTransitionToTraitCollection:(UITraitCollection*)newCollection
              withTransitionCoordinator:
                  (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super willTransitionToTraitCollection:newCollection
               withTransitionCoordinator:coordinator];
  // Invalidating the layout after changing the cellStyle results in the layout
  // not being updated. Do it before to have it taken into account.
  [self.collectionView.collectionViewLayout invalidateLayout];
  self.styler.cellStyle = MDCCollectionViewCellStyleCard;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self correctMissingSafeArea];
  [self updateOverscrollActionsState];
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  [self correctMissingSafeArea];
  [self.headerSynchronizer
      updateFakeOmniboxOnNewWidth:self.collectionView.bounds.size.width];
  [self.headerSynchronizer updateConstraints];
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  [self.headerSynchronizer unfocusOmnibox];

  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch ([self.collectionUpdater contentSuggestionTypeForItem:item]) {
    case ContentSuggestionTypeReadingList:
      base::RecordAction(base::UserMetricsAction("MobileReadingListOpen"));
      [self.suggestionCommandHandler openPageForItemAtIndexPath:indexPath];
      break;
    case ContentSuggestionTypeArticle:
      [self.suggestionCommandHandler openPageForItemAtIndexPath:indexPath];
      break;
    case ContentSuggestionTypeMostVisited:
      [self.suggestionCommandHandler openMostVisitedItem:item
                                                 atIndex:indexPath.item];
      break;
    case ContentSuggestionTypePromo:
      [self dismissSection:indexPath.section];
      [self.suggestionCommandHandler handlePromoTapped];
      [self.collectionViewLayout invalidateLayout];
      break;
    case ContentSuggestionTypeLearnMore:
      [self.suggestionCommandHandler handleLearnMoreTapped];
      break;
    case ContentSuggestionTypeEmpty:
      break;
  }
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  CSCollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];

  if ([self.collectionUpdater isContentSuggestionsSection:indexPath.section] &&
      [self.collectionUpdater contentSuggestionTypeForItem:item] !=
          ContentSuggestionTypeEmpty &&
      !item.metricsRecorded) {
    [self.metricsRecorder
            onSuggestionShown:item
                  atIndexPath:indexPath
        suggestionsShownAbove:[self
                                  numberOfSuggestionsAbove:indexPath.section]];
    item.metricsRecorded = YES;
  }

  return [super collectionView:collectionView cellForItemAtIndexPath:indexPath];
}

#pragma mark - UICollectionViewDelegateFlowLayout

- (CGSize)collectionView:(UICollectionView*)collectionView
                    layout:(UICollectionViewLayout*)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath*)indexPath {
  if ([self.collectionUpdater isMostVisitedSection:indexPath.section]) {
    return [ContentSuggestionsMostVisitedCell defaultSize];
  }
  CGSize size = [super collectionView:collectionView
                               layout:collectionViewLayout
               sizeForItemAtIndexPath:indexPath];
  // Special case for last item to add extra spacing before the footer.
  if ([self.collectionUpdater isContentSuggestionsSection:indexPath.section] &&
      indexPath.row ==
          [self.collectionView numberOfItemsInSection:indexPath.section] - 1)
    size.height += [ContentSuggestionsCell standardSpacing];

  return size;
}

- (UIEdgeInsets)collectionView:(UICollectionView*)collectionView
                        layout:(UICollectionViewLayout*)collectionViewLayout
        insetForSectionAtIndex:(NSInteger)section {
  UIEdgeInsets parentInset = [super collectionView:collectionView
                                            layout:collectionViewLayout
                            insetForSectionAtIndex:section];
  if ([self.collectionUpdater isHeaderSection:section]) {
    parentInset.top = 0;
    parentInset.left = 0;
    parentInset.right = 0;
  } else if ([self.collectionUpdater isMostVisitedSection:section] ||
             [self.collectionUpdater isPromoSection:section]) {
    CGFloat margin = content_suggestions::centeredTilesMarginForWidth(
        collectionView.frame.size.width);
    parentInset.left = margin;
    parentInset.right = margin;
    if ([self.collectionUpdater isMostVisitedSection:section]) {
      parentInset.bottom = kMostVisitedBottomMargin;
    }
  } else if (self.styler.cellStyle == MDCCollectionViewCellStyleCard) {
    CGFloat collectionWidth = collectionView.bounds.size.width;
    CGFloat maxCardWidth =
        content_suggestions::searchFieldWidth(collectionWidth);
    CGFloat margin =
        MAX(0, (collectionView.frame.size.width - maxCardWidth) / 2);
    parentInset.left = margin;
    parentInset.right = margin;
  }
  return parentInset;
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
                                 layout:(UICollectionViewLayout*)
                                            collectionViewLayout
    minimumLineSpacingForSectionAtIndex:(NSInteger)section {
  if ([self.collectionUpdater isMostVisitedSection:section]) {
    return content_suggestions::verticalSpacingBetweenTiles();
  }
  return [super collectionView:collectionView
                                   layout:collectionViewLayout
      minimumLineSpacingForSectionAtIndex:section];
}

#pragma mark - MDCCollectionViewStylingDelegate

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  return YES;
}

- (UIColor*)collectionView:(nonnull UICollectionView*)collectionView
    cellBackgroundColorAtIndexPath:(nonnull NSIndexPath*)indexPath {
  if ([self.collectionUpdater
          shouldUseCustomStyleForSection:indexPath.section]) {
    return [UIColor clearColor];
  }
  return ntp_home::kNTPBackgroundColor();
}

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  if ([self.collectionUpdater isHeaderSection:section]) {
    return CGSizeMake(0, [self.headerSynchronizer headerHeight]);
  }
  return [super collectionView:collectionView
                               layout:collectionViewLayout
      referenceSizeForHeaderInSection:section];
}

- (BOOL)collectionView:(nonnull UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(nonnull NSIndexPath*)indexPath {
  return
      [self.collectionUpdater shouldUseCustomStyleForSection:indexPath.section];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideHeaderBackgroundForSection:(NSInteger)section {
  return [self.collectionUpdater shouldUseCustomStyleForSection:section];
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CSCollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  UIEdgeInsets inset = [self collectionView:collectionView
                                     layout:collectionView.collectionViewLayout
                     insetForSectionAtIndex:indexPath.section];
  CGFloat width =
      CGRectGetWidth(collectionView.bounds) - inset.left - inset.right;

  return [item cellHeightForWidth:width];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemSeparatorAtIndexPath:(NSIndexPath*)indexPath {
  // Special case, show a seperator between the last regular item and the
  // footer.
  if (![self.collectionUpdater
          shouldUseCustomStyleForSection:indexPath.section] &&
      indexPath.row ==
          [self.collectionView numberOfItemsInSection:indexPath.section] - 1) {
    return NO;
  }
  return YES;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideHeaderSeparatorForSection:(NSInteger)section {
  return [self.collectionUpdater shouldUseCustomStyleForSection:section];
}

#pragma mark - MDCCollectionViewEditingDelegate

- (BOOL)collectionViewAllowsSwipeToDismissItem:
    (UICollectionView*)collectionView {
  return YES;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    canSwipeToDismissItemAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  return ![self.collectionUpdater isMostVisitedSection:indexPath.section] &&
         ![self.collectionUpdater isPromoSection:indexPath.section] &&
         [self.collectionUpdater contentSuggestionTypeForItem:item] !=
             ContentSuggestionTypeLearnMore &&
         [self.collectionUpdater contentSuggestionTypeForItem:item] !=
             ContentSuggestionTypeEmpty;
}

- (void)collectionView:(UICollectionView*)collectionView
    didEndSwipeToDismissItemAtIndexPath:(NSIndexPath*)indexPath {
  [self.collectionUpdater
      dismissItem:[self.collectionViewModel itemAtIndexPath:indexPath]];
  [self dismissEntryAtIndexPath:indexPath];
}

#pragma mark - UIScrollViewDelegate Methods.

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [super scrollViewDidScroll:scrollView];
  [self.overscrollActionsController scrollViewDidScroll:scrollView];
  [self.headerSynchronizer updateFakeOmniboxOnCollectionScroll];
  self.scrolledToTop =
      scrollView.contentOffset.y >= [self.headerSynchronizer pinnedOffsetY];
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  [self.overscrollActionsController scrollViewWillBeginDragging:scrollView];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  [super scrollViewDidEndDragging:scrollView willDecelerate:decelerate];
  [self.overscrollActionsController scrollViewDidEndDragging:scrollView
                                              willDecelerate:decelerate];
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  [super scrollViewWillEndDragging:scrollView
                      withVelocity:velocity
               targetContentOffset:targetContentOffset];
  [self.overscrollActionsController
      scrollViewWillEndDragging:scrollView
                   withVelocity:velocity
            targetContentOffset:targetContentOffset];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  return touch.view.accessibilityIdentifier !=
             ntp_home::FakeOmniboxAccessibilityID() &&
         touch.view.superview.accessibilityIdentifier !=
             ntp_home::FakeOmniboxAccessibilityID();
}

#pragma mark - UIAccessibilityAction

- (BOOL)accessibilityScroll:(UIAccessibilityScrollDirection)direction {
  // The collection displays the fake omnibox on the top of the other elements.
  // The default scrolling action scrolls for the full height of the collection,
  // hiding elements behing the fake omnibox. This reduces the scrolling by the
  // height of the fake omnibox.
  if (direction == UIAccessibilityScrollDirectionDown) {
    CGFloat newYOffset = self.collectionView.contentOffset.y +
                         self.collectionView.bounds.size.height -
                         ntp_header::ToolbarHeight();
    newYOffset = MIN(self.collectionView.contentSize.height -
                         self.collectionView.bounds.size.height,
                     newYOffset);
    self.collectionView.contentOffset =
        CGPointMake(self.collectionView.contentOffset.x, newYOffset);
  } else if (direction == UIAccessibilityScrollDirectionUp) {
    CGFloat newYOffset = self.collectionView.contentOffset.y -
                         self.collectionView.bounds.size.height +
                         ntp_header::ToolbarHeight();
    newYOffset = MAX(0, newYOffset);
    self.collectionView.contentOffset =
        CGPointMake(self.collectionView.contentOffset.x, newYOffset);
  } else {
    return NO;
  }
  return YES;
}

#pragma mark - Private

// TODO(crbug.com/826369) Remove this when the NTP is conatined by the BVC
// and removed from native content.  As a part of native content, the NTP is
// contained by a view controller that is inset from safeArea.top.  Even
// though content suggestions appear under the top safe area, they are blocked
// by the browser container view controller.
- (void)correctMissingSafeArea {
  if (base::FeatureList::IsEnabled(web::features::kBrowserContainerFullscreen))
    return;

  if (@available(iOS 11, *)) {
    UIEdgeInsets missingTop = UIEdgeInsetsZero;
    // During the new tab animation the browser container view controller
    // actually matches the browser view controller frame, so safe area does
    // work, so be sure to check the parent view controller offset.
    if (self.parentViewController.view.frame.origin.y == StatusBarHeight())
      missingTop = UIEdgeInsetsMake(StatusBarHeight(), 0, 0, 0);
    self.additionalSafeAreaInsets = missingTop;
  }
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
  CollectionViewItem* touchedItem =
      [self.collectionViewModel itemAtIndexPath:touchedItemIndexPath];

  ContentSuggestionType type =
      [self.collectionUpdater contentSuggestionTypeForItem:touchedItem];
  switch (type) {
    case ContentSuggestionTypeArticle:
      [self.suggestionCommandHandler
          displayContextMenuForSuggestion:touchedItem
                                  atPoint:touchLocation
                              atIndexPath:touchedItemIndexPath
                          readLaterAction:YES];
      break;
    case ContentSuggestionTypeReadingList:
      [self.suggestionCommandHandler
          displayContextMenuForSuggestion:touchedItem
                                  atPoint:touchLocation
                              atIndexPath:touchedItemIndexPath
                          readLaterAction:NO];
      break;
    case ContentSuggestionTypeMostVisited:
      [self.suggestionCommandHandler
          displayContextMenuForMostVisitedItem:touchedItem
                                       atPoint:touchLocation
                                   atIndexPath:touchedItemIndexPath];
      break;
    default:
      break;
  }

  if (IsRegularXRegularSizeClass(self))
    [self.headerSynchronizer unfocusOmnibox];
}

// Checks if the |section| is empty and add an empty element if it is the case.
// Must be called from inside a performBatchUpdates: block.
- (void)addEmptySectionPlaceholderIfNeeded:(NSInteger)section {
  if ([self.collectionViewModel numberOfItemsInSection:section] > 0)
    return;

  NSIndexPath* emptyItem =
      [self.collectionUpdater addEmptyItemForSection:section];
  if (emptyItem)
    [self.collectionView insertItemsAtIndexPaths:@[ emptyItem ]];
}

@end
