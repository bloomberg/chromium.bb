// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"

#include "base/format_macros.h"
#import "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_layout_util.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row_cell.h"
#include "ios/chrome/browser/ui/omnibox/popup/self_sizing_table_view.h"
#include "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kTopAndBottomPadding = 8.0;
}  // namespace

@interface OmniboxPopupViewController () <UITableViewDataSource,
                                          UITableViewDelegate,
                                          OmniboxPopupRowCellDelegate>

// Index path of currently highlighted row. The rows can be highlighted by
// tapping and holding on them or by using arrow keys on a hardware keyboard.
@property(nonatomic, strong) NSIndexPath* highlightedIndexPath;

// Flag that enables forwarding scroll events to the delegate. Disabled while
// updating the cells to avoid defocusing the omnibox when the omnibox popup
// changes size and table view issues a scroll event.
@property(nonatomic, assign) BOOL forwardsScrollEvents;

// The height of the keyboard. Used to determine the content inset for the
// scroll view.
@property(nonatomic, assign) CGFloat keyboardHeight;

// Time the view appeared on screen. Used to record a metric of how long this
// view controller was on screen.
@property(nonatomic, assign) base::TimeTicks viewAppearanceTime;
// Table view that displays the results.
@property(nonatomic, strong) UITableView* tableView;

// Alignment of omnibox text. Popup text should match this alignment.
@property(nonatomic, assign) NSTextAlignment alignment;

// Semantic content attribute of omnibox text. Popup should match this
// attribute. This is used by the new omnibox popup.
@property(nonatomic, assign)
    UISemanticContentAttribute semanticContentAttribute;

@end

@implementation OmniboxPopupViewController

- (instancetype)init {
  if (self = [super initWithNibName:nil bundle:nil]) {
    _forwardsScrollEvents = YES;
    if (IsIPadIdiom()) {
      // The iPad keyboard can cover some of the rows of the scroll view. The
      // scroll view's content inset may need to be updated when the keyboard is
      // displayed.
      NSNotificationCenter* defaultCenter =
          [NSNotificationCenter defaultCenter];
      [defaultCenter addObserver:self
                        selector:@selector(keyboardDidShow:)
                            name:UIKeyboardDidShowNotification
                          object:nil];
    }
  }
  return self;
}

- (void)loadView {
  self.tableView =
      [[SelfSizingTableView alloc] initWithFrame:CGRectZero
                                           style:UITableViewStylePlain];
  self.tableView.delegate = self;
  self.tableView.dataSource = self;
  self.view = self.tableView;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateBackgroundColor];
}

#pragma mark - View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier =
      kOmniboxPopupTableViewAccessibilityIdentifier;
  self.tableView.insetsContentViewsToSafeArea = YES;

  // Initialize the same size as the parent view, autoresize will correct this.
  [self.view setFrame:CGRectZero];
  [self.view setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                  UIViewAutoresizingFlexibleHeight)];

  [self updateBackgroundColor];

  // Table configuration.
  self.tableView.allowsMultipleSelectionDuringEditing = NO;
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  self.tableView.separatorInset = UIEdgeInsetsZero;
  if ([self.tableView respondsToSelector:@selector(setLayoutMargins:)]) {
    [self.tableView setLayoutMargins:UIEdgeInsetsZero];
  }
  self.tableView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentAutomatic;
  [self.tableView setContentInset:UIEdgeInsetsMake(kTopAndBottomPadding, 0,
                                                   kTopAndBottomPadding, 0)];
  self.tableView.estimatedRowHeight = 0;

  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = kOmniboxPopupCellMinimumHeight;

  [self.tableView registerClass:[OmniboxPopupRowCell class]
         forCellReuseIdentifier:OmniboxPopupRowCellReuseIdentifier];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  self.viewAppearanceTime = base::TimeTicks::Now();
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  UMA_HISTOGRAM_MEDIUM_TIMES("MobileOmnibox.PopupOpenDuration",
                             base::TimeTicks::Now() - self.viewAppearanceTime);
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [self.tableView setEditing:NO animated:NO];
}

#pragma mark - AutocompleteResultConsumer

- (void)updateMatches:(NSArray<id<AutocompleteSuggestion>>*)result
        withAnimation:(BOOL)animation {
  self.forwardsScrollEvents = NO;
  // Reset highlight state.
  if (self.highlightedIndexPath) {
    [self unhighlightRowAtIndexPath:self.highlightedIndexPath];
    self.highlightedIndexPath = nil;
  }

  self.currentResult = result;

  [self.tableView reloadData];
  self.forwardsScrollEvents = YES;
}

- (void)highlightRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  [cell setHighlighted:YES animated:NO];
}

- (void)unhighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  [cell setHighlighted:NO animated:NO];
}

// Set text alignment for popup cells.
- (void)setTextAlignment:(NSTextAlignment)alignment {
  self.alignment = alignment;
}

#pragma mark - OmniboxSuggestionCommands

- (void)highlightNextSuggestion {
  NSIndexPath* path = self.highlightedIndexPath;
  if (path == nil) {
    // When nothing is highlighted, pressing Up Arrow doesn't do anything.
    return;
  }

  if (path.row == 0) {
    // Can't move up from first row. Call the delegate again so that the inline
    // autocomplete text is set again (in case the user exited the inline
    // autocomplete).
    [self.delegate autocompleteResultConsumer:self
                              didHighlightRow:self.highlightedIndexPath.row];
    return;
  }

  [self unhighlightRowAtIndexPath:self.highlightedIndexPath];
  self.highlightedIndexPath =
      [NSIndexPath indexPathForRow:self.highlightedIndexPath.row - 1
                         inSection:0];
  [self highlightRowAtIndexPath:self.highlightedIndexPath];

  [self.delegate autocompleteResultConsumer:self
                            didHighlightRow:self.highlightedIndexPath.row];
}

- (void)highlightPreviousSuggestion {
  if (!self.highlightedIndexPath) {
    // Initialize the highlighted row to -1, so that pressing down when nothing
    // is highlighted highlights the first row (at index 0).
    self.highlightedIndexPath = [NSIndexPath indexPathForRow:-1 inSection:0];
  }

  NSIndexPath* path = self.highlightedIndexPath;

  if (path.row == [self.tableView numberOfRowsInSection:0] - 1) {
    // Can't go below last row. Call the delegate again so that the inline
    // autocomplete text is set again (in case the user exited the inline
    // autocomplete).
    [self.delegate autocompleteResultConsumer:self
                              didHighlightRow:self.highlightedIndexPath.row];
    return;
  }

  // There is a row below, move highlight there.
  [self unhighlightRowAtIndexPath:self.highlightedIndexPath];
  self.highlightedIndexPath =
      [NSIndexPath indexPathForRow:self.highlightedIndexPath.row + 1
                         inSection:0];
  [self highlightRowAtIndexPath:self.highlightedIndexPath];

  [self.delegate autocompleteResultConsumer:self
                            didHighlightRow:self.highlightedIndexPath.row];
}

- (void)keyCommandReturn {
  [self.tableView selectRowAtIndexPath:self.highlightedIndexPath
                              animated:YES
                        scrollPosition:UITableViewScrollPositionNone];
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView*)tableView
      willDisplayCell:(UITableViewCell*)cell
    forRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([cell isKindOfClass:[OmniboxPopupRowCell class]]) {
    OmniboxPopupRowCell* rowCell =
        base::mac::ObjCCastStrict<OmniboxPopupRowCell>(cell);
    // This has to be set here because the cell's content view has its
    // semantic content attribute reset before the cell is displayed (and before
    // this method is called).
    rowCell.omniboxSemanticContentAttribute = self.semanticContentAttribute;

    rowCell.accessibilityIdentifier = [NSString
        stringWithFormat:@"omnibox suggestion %" PRIuNS, indexPath.row];
  }
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  return YES;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);
  DCHECK_LT((NSUInteger)indexPath.row, self.currentResult.count);
  NSUInteger row = indexPath.row;

  // Crash reports tell us that |row| is sometimes indexed past the end of
  // the results array. In those cases, just ignore the request and return
  // early. See b/5813291.
  if (row >= self.currentResult.count)
    return;
  [self.delegate autocompleteResultConsumer:self didSelectRow:row];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  DCHECK_EQ(0, section);
  return self.currentResult.count;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);

  // iOS doesn't check -numberOfRowsInSection before checking
  // -canEditRowAtIndexPath in a reload call. If |indexPath.row| is too large,
  // simple return |NO|.
  if ((NSUInteger)indexPath.row >= self.currentResult.count)
    return NO;

  return [self.currentResult[indexPath.row] supportsDeletion];
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);
  DCHECK_LT((NSUInteger)indexPath.row, self.currentResult.count);
  if (editingStyle == UITableViewCellEditingStyleDelete) {
    [self.delegate autocompleteResultConsumer:self
                      didSelectRowForDeletion:indexPath.row];
  }
}

#pragma mark - Internal API methods

// Adjust the inset on the table view to prevent keyboard from overlapping the
// text.
- (void)updateContentInsetForKeyboard {
  CGRect absoluteRect =
      [self.tableView convertRect:self.tableView.bounds
                toCoordinateSpace:UIScreen.mainScreen.coordinateSpace];
  CGFloat screenHeight = CurrentScreenHeight();
  CGFloat bottomInset = screenHeight - self.tableView.contentSize.height -
                        _keyboardHeight - absoluteRect.origin.y -
                        kTopAndBottomPadding * 2;
  bottomInset = MAX(kTopAndBottomPadding, -bottomInset);
  self.tableView.contentInset =
      UIEdgeInsetsMake(kTopAndBottomPadding, 0, bottomInset, 0);
  self.tableView.scrollIndicatorInsets = self.tableView.contentInset;
}

// Updates the color of the background based on the incognito-ness and the size
// class.
- (void)updateBackgroundColor {
  ToolbarConfiguration* configuration = [[ToolbarConfiguration alloc]
      initWithStyle:self.incognito ? INCOGNITO : NORMAL];

  if (IsRegularXRegularSizeClass(self)) {
    self.view.backgroundColor = configuration.backgroundColor;
  } else {
    self.view.backgroundColor = [UIColor clearColor];
  }
}

#pragma mark Action for append UIButton

// Action handler for when the button is tapped.
- (void)trailingButtonTapped:(id)sender {
  NSUInteger row = [sender tag];
  [self.delegate autocompleteResultConsumer:self
                 didTapTrailingButtonForRow:row];
}

- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  _semanticContentAttribute = semanticContentAttribute;
  // If there are any visible cells, update them right away.
  for (UITableViewCell* cell in self.tableView.visibleCells) {
    if ([cell isKindOfClass:[OmniboxPopupRowCell class]]) {
      OmniboxPopupRowCell* rowCell =
          base::mac::ObjCCastStrict<OmniboxPopupRowCell>(cell);
      // This has to be set here because the cell's content view has its
      // semantic content attribute reset before the cell is displayed (and
      // before this method is called).
      rowCell.omniboxSemanticContentAttribute = self.semanticContentAttribute;
    }
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  // TODO(crbug.com/733650): Default to the dragging check once it's been tested
  // on trunk.
  if (!scrollView.dragging)
    return;

  // TODO(crbug.com/911534): The following call chain ultimately just dismisses
  // the keyboard, but involves many layers of plumbing, and should be
  // refactored.
  if (self.forwardsScrollEvents)
    [self.delegate autocompleteResultConsumerDidScroll:self];

  [self.tableView deselectRowAtIndexPath:self.tableView.indexPathForSelectedRow
                                animated:NO];
}


#pragma mark - Table view data source

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  return UITableViewAutomaticDimension;
}

// Customize the appearance of table view cells.
- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);

  DCHECK_LT((NSUInteger)indexPath.row, self.currentResult.count);
  OmniboxPopupRowCell* cell = [self.tableView
      dequeueReusableCellWithIdentifier:OmniboxPopupRowCellReuseIdentifier
                           forIndexPath:indexPath];
  cell.faviconRetriever = self.faviconRetriever;
  cell.imageRetriever = self.imageRetriever;
  [cell setupWithAutocompleteSuggestion:self.currentResult[indexPath.row]
                              incognito:self.incognito];
  cell.showsSeparator =
      (NSUInteger)indexPath.row < self.currentResult.count - 1;
  cell.delegate = self;

  return cell;
}

#pragma mark - OmniboxPopupRowCellDelegate

- (void)trailingButtonTappedForCell:(OmniboxPopupRowCell*)cell {
  NSIndexPath* indexPath = [self.tableView indexPathForCell:cell];
  [self.delegate autocompleteResultConsumer:self
                 didTapTrailingButtonForRow:indexPath.row];
}

#pragma mark - keyboard events

- (void)keyboardDidShow:(NSNotification*)notification {
  NSDictionary* keyboardInfo = [notification userInfo];
  NSValue* keyboardFrameValue =
      [keyboardInfo valueForKey:UIKeyboardFrameEndUserInfoKey];
  self.keyboardHeight = CurrentKeyboardHeight(keyboardFrameValue);
  if (self.tableView.contentSize.height > 0)
    [self updateContentInsetForKeyboard];
}

@end
