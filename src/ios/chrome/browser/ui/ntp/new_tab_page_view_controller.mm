// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizing.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_layout.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/ntp/feed_header_view_controller.h"
#import "ios/chrome/browser/ui/ntp/feed_menu_commands.h"
#import "ios/chrome/browser/ui/ntp/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_omnibox_positioning.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewTabPageViewController () <NewTabPageOmniboxPositioning,
                                        UICollectionViewDelegate,
                                        UIGestureRecognizerDelegate>

// The overscroll actions controller managing accelerators over the toolbar.
@property(nonatomic, strong)
    OverscrollActionsController* overscrollActionsController;

// Whether or not the user has scrolled into the feed, transferring ownership of
// the omnibox to allow it to stick to the top of the NTP.
// With Web Channels enabled, also determines if the feed header is stuck to the
// top.
// TODO(crbug.com/1277504): Modify this comment when Web Channels is released.
@property(nonatomic, assign, getter=isScrolledIntoFeed) BOOL scrolledIntoFeed;

// The collection view layout for the uppermost content suggestions collection
// view.
@property(nonatomic, weak) ContentSuggestionsLayout* contentSuggestionsLayout;

// Constraint to determine the height of the contained ContentSuggestions view.
@property(nonatomic, strong)
    NSLayoutConstraint* contentSuggestionsHeightConstraint;

// Array of constraints used to pin the fake Omnibox header into the top of the
// view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* fakeOmniboxConstraints;

// Array of constraints used to pin the feed header to the top of the NTP. Only
// applicable with Web Channels enabled.
// TODO(crbug.com/1277504): Modify this comment when Web Channels is released.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* feedHeaderConstraints;

// Whether or not this NTP has fully appeared for the first time yet. This value
// remains YES if viewDidAppear has been called.
@property(nonatomic, assign) BOOL viewDidAppear;

// |YES| if the initial scroll position is from the saved web state (when
// navigating away and back), and |NO| if it is the top of the NTP.
@property(nonatomic, assign, getter=isInitialOffsetFromSavedState)
    BOOL initialOffsetFromSavedState;

// The scroll position when a scrolling event starts.
@property(nonatomic, assign) int scrollStartPosition;

// Whether the omnibox should be focused once the collection view appears.
@property(nonatomic, assign) BOOL shouldFocusFakebox;

@end

@implementation NewTabPageViewController

// Synthesized for ContentSuggestionsCollectionControlling protocol.
@synthesize headerSynchronizer = _headerSynchronizer;
@synthesize scrolledToMinimumHeight = _scrolledToMinimumHeight;

- (instancetype)init {
  return [super initWithNibName:nil bundle:nil];
}

- (void)dealloc {
  [self.overscrollActionsController invalidate];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  DCHECK(self.discoverFeedWrapperViewController);
  DCHECK(self.contentSuggestionsViewController);

  // TODO(crbug.com/1262536): Remove this when bug is fixed.
  [self.discoverFeedWrapperViewController loadViewIfNeeded];
  [self.contentSuggestionsViewController loadViewIfNeeded];

  // Prevent the NTP from spilling behind the toolbar and tab strip.
  self.view.clipsToBounds = YES;

  UIView* discoverFeedView = self.discoverFeedWrapperViewController.view;

  self.collectionView.accessibilityIdentifier = kNTPCollectionViewIdentifier;

  [self.discoverFeedWrapperViewController willMoveToParentViewController:self];
  [self addChildViewController:self.discoverFeedWrapperViewController];
  [self.view addSubview:discoverFeedView];
  [self.discoverFeedWrapperViewController didMoveToParentViewController:self];

  discoverFeedView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(discoverFeedView, self.view);

  UIViewController* parentViewController =
      self.isFeedVisible ? self.discoverFeedWrapperViewController.discoverFeed
                         : self.discoverFeedWrapperViewController;

  if (self.contentSuggestionsViewController.parentViewController) {
    [self.contentSuggestionsViewController willMoveToParentViewController:nil];
    [self.contentSuggestionsViewController.view removeFromSuperview];
    [self.contentSuggestionsViewController removeFromParentViewController];
    [self.feedMetricsRecorder
        recordBrokenNTPHierarchy:BrokenNTPHierarchyRelationship::
                                     kContentSuggestionsReset];
  }

  [self.contentSuggestionsViewController
      willMoveToParentViewController:parentViewController];
  [parentViewController
      addChildViewController:self.contentSuggestionsViewController];
  [self.collectionView addSubview:self.contentSuggestionsViewController.view];
  [self.contentSuggestionsViewController
      didMoveToParentViewController:parentViewController];

  // TODO(crbug.com/1170995): The contentCollectionView width might be narrower
  // than the ContentSuggestions view. This causes elements to be hidden. As a
  // temporary workaround set clipsToBounds to NO to display these elements, and
  // add a gesture recognizer to interact with them.
  self.collectionView.clipsToBounds = NO;
  UITapGestureRecognizer* singleTapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleSingleTapInView:)];
  singleTapRecognizer.delegate = self;
  [self.view addGestureRecognizer:singleTapRecognizer];

  // Ensures that there is never any nested scrolling, since we are nesting the
  // content suggestions collection view in the feed collection view.
  self.contentSuggestionsViewController.collectionView.bounces = NO;
  self.contentSuggestionsViewController.collectionView.alwaysBounceVertical =
      NO;
  self.contentSuggestionsViewController.collectionView.scrollEnabled = NO;

  [self configureOverscrollActionsController];

  self.view.backgroundColor = ntp_home::kNTPBackgroundColor();

  _contentSuggestionsLayout = static_cast<ContentSuggestionsLayout*>(
      self.contentSuggestionsViewController.collectionView
          .collectionViewLayout);
  _contentSuggestionsLayout.isScrolledIntoFeed = self.isScrolledIntoFeed;
  _contentSuggestionsLayout.omniboxPositioner = self;
  _contentSuggestionsLayout.parentCollectionView = self.collectionView;

  // If the feed is not visible, we control the delegate ourself (since it is
  // otherwise controlled by the DiscoverProvider).
  if (!self.isFeedVisible) {
    self.discoverFeedWrapperViewController.contentCollectionView.delegate =
        self;
  }

  [self registerNotifications];

  // Sets up the feed header if it is visible.
  if (self.feedHeaderViewController) {
    [self.feedHeaderViewController
        willMoveToParentViewController:parentViewController];
    [parentViewController addChildViewController:self.feedHeaderViewController];
    [self.collectionView addSubview:self.feedHeaderViewController.view];
    [self.feedHeaderViewController
        didMoveToParentViewController:parentViewController];
    [self.feedHeaderViewController.menuButton
               addTarget:self.feedMenuHandler
                  action:@selector(openFeedMenu)
        forControlEvents:UIControlEventTouchUpInside];
  }
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];

  [self updateNTPLayout];
  [self updateHeaderSynchronizerOffset];
  [self updateScrolledToMinimumHeight];
  [self.headerSynchronizer updateConstraints];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  self.headerSynchronizer.showing = YES;

  // Set these constraints in viewWillAppear so ContentSuggestions View uses its
  // intrinsic height in the initial layout instead of
  // contentSuggestionsHeightConstraint. If this is not done the
  // ContentSuggestions View will look broken for a second before its properly
  // laid out.
  if (!self.contentSuggestionsHeightConstraint) {
    [self applyCollectionViewConstraints];
  }

  [self updateNTPLayout];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // Updates omnibox to ensure that the dimensions are correct when navigating
  // back to the NTP.
  [self.headerSynchronizer updateFakeOmniboxForScrollPosition];

  if (self.shouldFocusFakebox && [self collectionViewHasLoaded]) {
    [self.headerController focusFakebox];
    self.shouldFocusFakebox = NO;
  }

  if (!self.isFeedVisible) {
    [self setMinimumHeight];
  }

  [self.bubblePresenter presentDiscoverFeedHeaderTipBubble];

  self.viewDidAppear = YES;
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  self.headerSynchronizer.showing = NO;
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];

  [self.headerSynchronizer updateConstraints];
  // Only update the insets if this NTP is being viewed for this first time. If
  // we are reopening an existing NTP, the insets are already ok.
  // TODO(crbug.com/1170995): Remove this once we use a custom feed header.
  if (!self.viewDidAppear) {
    [self updateFeedInsetsForContentAbove];
  }
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  __weak NewTabPageViewController* weakSelf = self;

  CGFloat yOffsetBeforeRotation = self.collectionView.contentOffset.y;
  CGFloat heightAboveFeedBeforeRotation = [self heightAboveFeed];

  void (^alongsideBlock)(id<UIViewControllerTransitionCoordinatorContext>) = ^(
      id<UIViewControllerTransitionCoordinatorContext> context) {
    [weakSelf handleStickyElementsForScrollPosition:weakSelf.collectionView
                                                        .contentOffset.y
                                              force:YES];

    // Redraw the ContentSuggestionsViewController to properly caclculate the
    // new adjustedContentSuggestionsHeight value.
    // TODO(crbug.com/1170995): Remove once the Feed supports a custom
    // header.
    [self.contentSuggestionsViewController.view setNeedsLayout];
    [self.contentSuggestionsViewController.view layoutIfNeeded];

    CGFloat heightAboveFeedDifference =
        [self heightAboveFeed] - heightAboveFeedBeforeRotation;

    // Rotating the device can change the content suggestions height. This
    // ensures that it is adjusted if necessary.
    if (yOffsetBeforeRotation < 0) {
      weakSelf.collectionView.contentOffset =
          CGPointMake(0, yOffsetBeforeRotation - heightAboveFeedDifference);
      [weakSelf updateNTPLayout];
    } else {
      [weakSelf.contentSuggestionsViewController.collectionView
              .collectionViewLayout invalidateLayout];
    }
    [weakSelf.view setNeedsLayout];
    [weakSelf.view layoutIfNeeded];

    // Pinned offset is different based on the orientation, so we reevaluate the
    // minimum scroll position upon device rotation.
    CGFloat pinnedOffsetY = [weakSelf.headerSynchronizer pinnedOffsetY];
    if ([weakSelf.headerSynchronizer isOmniboxFocused] &&
        weakSelf.collectionView.contentOffset.y < pinnedOffsetY) {
      weakSelf.collectionView.contentOffset = CGPointMake(0, pinnedOffsetY);
    }
    if (!self.isFeedVisible) {
      [self setMinimumHeight];
    }
  };
  [coordinator
      animateAlongsideTransition:alongsideBlock
                      completion:^(
                          id<UIViewControllerTransitionCoordinatorContext>) {
                        [self updateFeedInsetsForContentAbove];
                      }];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (previousTraitCollection.horizontalSizeClass !=
      self.traitCollection.horizontalSizeClass) {
    [self.contentSuggestionsViewController.view setNeedsLayout];
    [self.contentSuggestionsViewController.view layoutIfNeeded];
    [self.ntpContentDelegate reloadContentSuggestions];
  }

  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    [self.contentSuggestionsViewController.collectionView
            .collectionViewLayout invalidateLayout];
    [self.headerSynchronizer updateFakeOmniboxForScrollPosition];
  }

  [self.headerSynchronizer updateConstraints];
  [self updateOverscrollActionsState];
}

#pragma mark - Public

- (void)willUpdateSnapshot {
  [self.overscrollActionsController clear];
}

- (void)stopScrolling {
  UIScrollView* scrollView = self.collectionView;
  [scrollView setContentOffset:scrollView.contentOffset animated:NO];
}

- (void)setSavedContentOffset:(CGFloat)offset {
  self.initialOffsetFromSavedState = YES;
  [self setContentOffset:offset];
}

- (void)setContentOffsetToTop {
  [self setContentOffset:-[self heightAboveFeed]];
  [self resetFakeOmnibox];
}

- (BOOL)isNTPScrolledToTop {
  return self.collectionView.contentOffset.y <= -[self heightAboveFeed];
}

- (void)updateNTPLayout {
  [self updateFeedInsetsForContentAbove];

  // Reload data to ensure the Most Visited tiles and fake omnibox are correctly
  // positioned, in particular during a rotation while a ViewController is
  // presented in front of the NTP.
  [self.headerSynchronizer
      updateFakeOmniboxOnNewWidth:self.collectionView.bounds.size.width];
  [self.contentSuggestionsViewController.collectionView
          .collectionViewLayout invalidateLayout];
  // Ensure initial fake omnibox layout.
  [self.headerSynchronizer updateFakeOmniboxForScrollPosition];

  if (!self.viewDidAppear && ![self isInitialOffsetFromSavedState]) {
    [self setContentOffsetToTop];
  }
}

- (void)focusFakebox {
  // The fakebox should only be focused once the collection view has reached its
  // minimum height. If this is not the case yet, we wait until viewDidAppear
  // before focusing the fakebox.
  if ([self collectionViewHasLoaded]) {
    [self.headerController focusFakebox];
  } else {
    self.shouldFocusFakebox = YES;
  }
}

- (CGFloat)heightAboveFeed {
  return [self adjustedContentSuggestionsHeight] + [self feedHeaderHeight];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  // Scroll events should not be handled until the content suggestions have been
  // layed out.
  if (!self.contentSuggestionsViewController.collectionView.contentSize
           .height) {
    return;
  }

  [self.overscrollActionsController scrollViewDidScroll:scrollView];
  [self.panGestureHandler scrollViewDidScroll:scrollView];
  [self.headerSynchronizer updateFakeOmniboxForScrollPosition];

  [self updateScrolledToMinimumHeight];

  CGFloat scrollPosition = scrollView.contentOffset.y;
  // Fixes the content suggestions collection view layout so that the header
  // scrolls at the same rate as the rest.
  if (scrollPosition > -[self heightAboveFeed]) {
    [self.contentSuggestionsViewController.collectionView
            .collectionViewLayout invalidateLayout];
  }
  [self handleStickyElementsForScrollPosition:scrollPosition force:NO];
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  [self.overscrollActionsController scrollViewWillBeginDragging:scrollView];
  [self.panGestureHandler scrollViewWillBeginDragging:scrollView];
  self.scrollStartPosition = scrollView.contentOffset.y;
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  [self.overscrollActionsController
      scrollViewWillEndDragging:scrollView
                   withVelocity:velocity
            targetContentOffset:targetContentOffset];
  [self.panGestureHandler scrollViewWillEndDragging:scrollView
                                       withVelocity:velocity
                                targetContentOffset:targetContentOffset];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  [self.overscrollActionsController scrollViewDidEndDragging:scrollView
                                              willDecelerate:decelerate];
  [self.panGestureHandler scrollViewDidEndDragging:scrollView
                                    willDecelerate:decelerate];
  [self.feedMetricsRecorder
      recordFeedScrolled:scrollView.contentOffset.y - self.scrollStartPosition];
}

- (void)scrollViewDidScrollToTop:(UIScrollView*)scrollView {
  // TODO(crbug.com/1114792): Handle scrolling.
}

- (void)scrollViewWillBeginDecelerating:(UIScrollView*)scrollView {
  // TODO(crbug.com/1114792): Handle scrolling.
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  // TODO(crbug.com/1114792): Handle scrolling.
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView {
  // TODO(crbug.com/1114792): Handle scrolling.
}

- (BOOL)scrollViewShouldScrollToTop:(UIScrollView*)scrollView {
  // User has tapped the status bar to scroll to the top.
  // Prevent scrolling back to pre-focus state, making sure we don't have
  // two scrolling animations running at the same time.
  [self.headerSynchronizer resetPreFocusOffset];
  // Unfocus omnibox without scrolling back.
  [self.headerSynchronizer unfocusOmnibox];
  return YES;
}

#pragma mark - ContentSuggestionsCollectionControlling

- (UICollectionView*)collectionView {
  return self.discoverFeedWrapperViewController.contentCollectionView;
}

#pragma mark - NewTabPageOmniboxPositioning

- (CGFloat)stickyOmniboxHeight {
  // Takes the height of the entire header and subtracts the margin to stick the
  // fake omnibox. Adjusts this for the device by further subtracting the
  // toolbar height and safe area insets.
  return self.headerController.view.frame.size.height -
         ntp_header::kFakeOmniboxScrolledToTopMargin -
         ToolbarExpandedHeight(
             [UIApplication sharedApplication].preferredContentSizeCategory) -
         self.view.safeAreaInsets.top - [self feedHeaderHeight];
}

#pragma mark - ThumbStripSupporting

- (BOOL)isThumbStripEnabled {
  return self.panGestureHandler != nil;
}

- (void)thumbStripEnabledWithPanHandler:
    (ViewRevealingVerticalPanHandler*)panHandler {
  DCHECK(!self.thumbStripEnabled);
  self.panGestureHandler = panHandler;
}

- (void)thumbStripDisabled {
  DCHECK(self.thumbStripEnabled);
  self.panGestureHandler = nil;
}

#pragma mark - UIGestureRecognizerDelegate

// TODO(crbug.com/1170995): Remove once the Feed header properly supports
// ContentSuggestions.
- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  // Ignore all touches inside the Feed CollectionView, which includes
  // ContentSuggestions.
  UIView* viewToIgnoreTouches = self.collectionView;
  CGRect ignoreBoundsInView =
      [viewToIgnoreTouches convertRect:viewToIgnoreTouches.bounds
                                toView:self.view];
  return !(CGRectContainsPoint(ignoreBoundsInView,
                               [touch locationInView:self.view]));
}

#pragma mark - Private

// Configures overscroll actions controller.
- (void)configureOverscrollActionsController {
  // Ensure the feed's scroll view exists to prevent crashing the overscroll
  // controller.
  if (!self.collectionView) {
    return;
  }
  // Overscroll action does not work well with content offset, so set this
  // to never and internally offset the UI to account for safe area insets.
  self.collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;

  self.overscrollActionsController = [[OverscrollActionsController alloc]
      initWithScrollView:self.collectionView];
  [self.overscrollActionsController
      setStyle:OverscrollStyle::NTP_NON_INCOGNITO];
  self.overscrollActionsController.delegate = self.overscrollDelegate;
  [self updateOverscrollActionsState];
}

// Enables or disables overscroll actions.
- (void)updateOverscrollActionsState {
  if (IsSplitToolbarMode(self)) {
    [self.overscrollActionsController enableOverscrollActions];
  } else {
    [self.overscrollActionsController disableOverscrollActions];
  }
}

// Pins sticky elements to the top of the NTP. This includes the fake omnibox
// and if Web Channels is enabled, the feed header.
// TODO(crbug.com/1277504): Modify this comment when Web Channels is released.
- (void)pinStickyElements {
  [self setIsScrolledIntoFeed:YES];

  [self stickFakeOmniboxToTop];

  if (IsWebChannelsEnabled()) {
    [self stickFeedHeaderToTop];
  }
}

// Resets the sticky elements to their original position. This includes the fake
// omnibox and if Web Channels is enabled, the feed header.
// TODO(crbug.com/1277504): Modify this comment when Web Channels is released.
- (void)resetStickyElements {
  [self setIsScrolledIntoFeed:NO];

  [self resetFakeOmnibox];

  if (IsWebChannelsEnabled()) {
    [self setInitialFeedHeaderConstraints];
  }
}

// Lets this view own the fake omnibox and sticks it to the top of the NTP.
- (void)stickFakeOmniboxToTop {
  [self.headerController removeFromParentViewController];
  [self.headerController.view removeFromSuperview];

  // If |self.headerController| is nil after removing it from the view hierarchy
  // it means its no longer owned by anyone (e.g. The coordinator might have
  // been stopped.) and we shouldn't try to add it again.
  if (!self.headerController)
    return;

  [self.view addSubview:self.headerController.view];

  self.fakeOmniboxConstraints = @[
    [self.headerController.view.topAnchor
        constraintEqualToAnchor:self.discoverFeedWrapperViewController.view
                                    .topAnchor
                       constant:-([self stickyOmniboxHeight] +
                                  [self feedHeaderHeight])],
    [self.headerController.view.leadingAnchor
        constraintEqualToAnchor:self.discoverFeedWrapperViewController.view
                                    .leadingAnchor],
    [self.headerController.view.trailingAnchor
        constraintEqualToAnchor:self.discoverFeedWrapperViewController.view
                                    .trailingAnchor],
    [self.headerController.view.heightAnchor
        constraintEqualToConstant:self.headerController.view.frame.size.height],
  ];

  self.contentSuggestionsHeightConstraint.active = NO;
  [NSLayoutConstraint activateConstraints:self.fakeOmniboxConstraints];
}

// Gives content suggestions collection view ownership of the fake omnibox for
// the width animation.
- (void)resetFakeOmnibox {
  [self.headerController removeFromParentViewController];
  [self.headerController.view removeFromSuperview];

  self.contentSuggestionsHeightConstraint.active = YES;
  [NSLayoutConstraint deactivateConstraints:self.fakeOmniboxConstraints];

  // Reload the content suggestions so that the fake omnibox goes back where it
  // belongs. This can probably be optimized by just reloading the header, if
  // that doesn't mess up any collection/header interactions.
  [self.ntpContentDelegate reloadContentSuggestions];
}

// Pins feed header to top of the NTP when scrolled into the feed, below the
// omnibox.
- (void)stickFeedHeaderToTop {
  [NSLayoutConstraint deactivateConstraints:self.feedHeaderConstraints];

  self.feedHeaderConstraints = @[
    [self.feedHeaderViewController.view.topAnchor
        constraintEqualToAnchor:self.headerController.view.bottomAnchor],
  ];

  [NSLayoutConstraint activateConstraints:self.feedHeaderConstraints];
}

// Sets initial feed header constraints, between content suggestions and feed.
- (void)setInitialFeedHeaderConstraints {
  [NSLayoutConstraint deactivateConstraints:self.feedHeaderConstraints];
  self.feedHeaderConstraints = @[
    [self.feedHeaderViewController.view.topAnchor
        constraintEqualToAnchor:self.contentSuggestionsViewController.view
                                    .bottomAnchor],
    [self.collectionView.topAnchor
        constraintEqualToAnchor:self.feedHeaderViewController.view
                                    .bottomAnchor],
  ];
  [NSLayoutConstraint activateConstraints:self.feedHeaderConstraints];
}

// Sets an inset to the feed equal to the height of the content above the feed,
// then place the content above the feed in this space.
- (void)updateFeedInsetsForContentAbove {
  // Adds inset to feed to create space for content above feed.
  self.collectionView.contentInset =
      UIEdgeInsetsMake([self heightAboveFeed], 0, 0, 0);

  // Sets frame for feed header and content suggestions within the space from
  // the inset.
  if (self.feedHeaderViewController) {
    self.feedHeaderViewController.view.frame =
        CGRectMake(self.feedHeaderViewController.view.frame.origin.x,
                   -[self feedHeaderHeight], self.view.frame.size.width,
                   [self feedHeaderHeight]);
  }
  self.contentSuggestionsViewController.view.frame = CGRectMake(
      self.contentSuggestionsViewController.view.frame.origin.x,
      -[self contentSuggestionsContentHeight] - [self feedHeaderHeight],
      self.view.frame.size.width, [self contentSuggestionsContentHeight]);

  self.contentSuggestionsHeightConstraint.constant =
      [self contentSuggestionsContentHeight];
  [self updateHeaderSynchronizerOffset];
}

// Updates headerSynchronizer's additionalOffset using the content above the
// feed.
- (void)updateHeaderSynchronizerOffset {
  self.headerSynchronizer.additionalOffset = [self heightAboveFeed];
}

// TODO(crbug.com/1170995): Remove once the Feed header properly supports
// ContentSuggestions.
- (void)handleSingleTapInView:(UITapGestureRecognizer*)recognizer {
  CGPoint location = [recognizer locationInView:[recognizer.view superview]];
  CGRect discBoundsInView =
      [self.identityDiscButton convertRect:self.identityDiscButton.bounds
                                    toView:self.view];
  if (CGRectContainsPoint(discBoundsInView, location)) {
    [self.identityDiscButton
        sendActionsForControlEvents:UIControlEventTouchUpInside];
  } else {
    [self.headerSynchronizer unfocusOmnibox];
  }
}

// Handles the pinning of the sticky elements to the top of the NTP. This
// includes the fake omnibox and if Web Channels is enabled, the feed header. If
// |force| is YES, the sticky elements will always be set based on the scroll
// position. If |force| is NO, the sticky elements will only based on
// |isScrolledIntoFeed| to prevent pinning them multiple times.
// TODO(crbug.com/1277504): Modify this comment when Web Channels is released.
- (void)handleStickyElementsForScrollPosition:(CGFloat)scrollPosition
                                        force:(BOOL)force {
  if ((!self.isScrolledIntoFeed || force) &&
      scrollPosition > [self offsetToStickOmniboxAndHeader]) {
    [self pinStickyElements];
  } else if ((self.isScrolledIntoFeed || force) &&
             scrollPosition <= [self offsetToStickOmniboxAndHeader]) {
    [self resetStickyElements];
  }

  // Content suggestions header will sometimes glitch when swiping quickly from
  // inside the feed to the top of the NTP. This check safeguards this action to
  // make sure the header is properly positioned. (crbug.com/1261458)
  if ([self isNTPScrolledToTop]) {
    [self resetFakeOmnibox];
  }
}

// Registers notifications for certain actions on the NTP.
- (void)registerNotifications {
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(deviceOrientationDidChange)
                 name:UIDeviceOrientationDidChangeNotification
               object:nil];
}

// Handles device rotation.
- (void)deviceOrientationDidChange {
  if (self.viewDidAppear) {
    [self.feedMetricsRecorder
        recordDeviceOrientationChanged:[[UIDevice currentDevice] orientation]];
  }
}

// Applies constraints to the NTP collection view, along with the constraints
// for the content suggestions within it.
- (void)applyCollectionViewConstraints {
  UIView* contentSuggestionsView = self.contentSuggestionsViewController.view;
  contentSuggestionsView.translatesAutoresizingMaskIntoConstraints = NO;

  self.contentSuggestionsHeightConstraint = [contentSuggestionsView.heightAnchor
      constraintEqualToConstant:self.contentSuggestionsViewController
                                    .collectionView.contentSize.height];

  if (self.feedHeaderViewController) {
    [NSLayoutConstraint activateConstraints:@[
      [self.feedHeaderViewController.view.leadingAnchor
          constraintEqualToAnchor:[self containerView].leadingAnchor],
      [self.feedHeaderViewController.view.trailingAnchor
          constraintEqualToAnchor:[self containerView].trailingAnchor],
    ]];
    [self setInitialFeedHeaderConstraints];
  } else {
    [NSLayoutConstraint activateConstraints:@[
      [self.collectionView.topAnchor
          constraintEqualToAnchor:contentSuggestionsView.bottomAnchor],
    ]];
  }

  [NSLayoutConstraint activateConstraints:@[
    [[self containerView].safeAreaLayoutGuide.leadingAnchor
        constraintEqualToAnchor:contentSuggestionsView.leadingAnchor],
    [[self containerView].safeAreaLayoutGuide.trailingAnchor
        constraintEqualToAnchor:contentSuggestionsView.trailingAnchor],
    self.contentSuggestionsHeightConstraint,
  ]];
}

// Sets minimum height for the NTP collection view, allowing it to scroll enough
// to focus the omnibox.
- (void)setMinimumHeight {
  self.collectionView.contentSize =
      CGSizeMake(self.view.frame.size.width,
                 [self.contentSuggestionsLayout minimumNTPHeight] -
                     [self heightAboveFeed]);
}

#pragma mark - Helpers

// Returns the current height of the content suggestions content.
- (CGFloat)contentSuggestionsContentHeight {
  return self.contentSuggestionsViewController.collectionView.contentSize
      .height;
}

// Content suggestions height adjusted with the safe area top insets.
- (CGFloat)adjustedContentSuggestionsHeight {
  return [self contentSuggestionsContentHeight] + self.view.safeAreaInsets.top;
}

// Height of the feed header, returns 0 if it is not visible.
- (CGFloat)feedHeaderHeight {
  return self.feedHeaderViewController
             ? self.feedHeaderViewController.view.frame.size.height
             : 0;
}

// The y-position content offset for when the feed header and fake omnibox
// should stick to the top of the NTP.
- (CGFloat)offsetToStickOmniboxAndHeader {
  return -(self.headerController.view.frame.size.height -
           [self stickyOmniboxHeight]);
}

// Whether the collection view has attained its minimum height.
// The fake omnibox never actually disappears; the NTP just scrolls enough so
// that it's hidden behind the real one when it's focused. When the NTP hasn't
// fully loaded yet, there isn't enough height to scroll it behind the real
// omnibox, so they would both show.
- (BOOL)collectionViewHasLoaded {
  return self.collectionView.contentSize.height > 0;
}

// TODO(crbug.com/1262536): Temporary fix to compensate for the view hierarchy
// sometimes breaking. Use DCHECKs to investigate what exactly is broken and
// find a fix.
- (void)verifyNTPViewHierarchy {
  // The view hierarchy with the feed enabled should be: self.view ->
  // self.discoverFeedWrapperViewController.view ->
  // self.discoverFeedWrapperViewController.discoverFeed.view ->
  // self.collectionView -> self.contentSuggestionsViewController.view.
  if (![self.collectionView.subviews
          containsObject:self.contentSuggestionsViewController.view]) {
    // Remove child VC from old parent.
    [self.contentSuggestionsViewController willMoveToParentViewController:nil];
    [self.contentSuggestionsViewController removeFromParentViewController];
    [self.contentSuggestionsViewController.view removeFromSuperview];
    [self.contentSuggestionsViewController didMoveToParentViewController:nil];

    // Add child VC to new parent.
    [self.contentSuggestionsViewController
        willMoveToParentViewController:self.discoverFeedWrapperViewController
                                           .discoverFeed];
    [self.discoverFeedWrapperViewController.discoverFeed
        addChildViewController:self.contentSuggestionsViewController];
    [self.collectionView addSubview:self.contentSuggestionsViewController.view];
    [self.contentSuggestionsViewController
        didMoveToParentViewController:self.discoverFeedWrapperViewController
                                          .discoverFeed];

    [self.feedMetricsRecorder
        recordBrokenNTPHierarchy:BrokenNTPHierarchyRelationship::
                                     kContentSuggestionsParent];
  }
  [self ensureView:self.feedHeaderViewController.view
             isSubviewOf:self.collectionView
      withRelationshipID:BrokenNTPHierarchyRelationship::kFeedHeaderParent];
  [self ensureView:self.collectionView
             isSubviewOf:self.discoverFeedWrapperViewController.discoverFeed
                             .view
      withRelationshipID:BrokenNTPHierarchyRelationship::kELMCollectionParent];
  [self ensureView:self.discoverFeedWrapperViewController.discoverFeed.view
             isSubviewOf:self.discoverFeedWrapperViewController.view
      withRelationshipID:BrokenNTPHierarchyRelationship::kDiscoverFeedParent];
  [self ensureView:self.discoverFeedWrapperViewController.view
             isSubviewOf:self.view
      withRelationshipID:BrokenNTPHierarchyRelationship::
                             kDiscoverFeedWrapperParent];
}

// Ensures that |subView| is a descendent of |parentView|. If not, logs a DCHECK
// and adds the subview. Includes |relationshipID| for metrics recorder to log
// which part of the view hierarchy was broken.
// TODO(crbug.com/1262536): Remove this once bug is fixed.
- (void)ensureView:(UIView*)subView
           isSubviewOf:(UIView*)parentView
    withRelationshipID:(BrokenNTPHierarchyRelationship)relationship {
  if (![parentView.subviews containsObject:subView]) {
    DCHECK([parentView.subviews containsObject:subView]);
    [subView removeFromSuperview];
    [parentView addSubview:subView];
    [self.feedMetricsRecorder recordBrokenNTPHierarchy:relationship];
  }
}

// Checks if the collection view is scrolled at least to the minimum height and
// updates property.
- (void)updateScrolledToMinimumHeight {
  CGFloat pinnedOffsetY = [self.headerSynchronizer pinnedOffsetY];
  self.scrolledToMinimumHeight =
      self.collectionView.contentOffset.y >= pinnedOffsetY;
}

#pragma mark - Getters

// Returns the container view of the NTP content, depending on prefs and flags.
- (UIView*)containerView {
  UIView* containerView;
  if (self.isFeedVisible) {
    // TODO(crbug.com/1262536): Remove this when the bug is fixed.
    if (IsNTPViewHierarchyRepairEnabled()) {
      [self verifyNTPViewHierarchy];
    }
    containerView = self.discoverFeedWrapperViewController.discoverFeed.view;
  } else {
    containerView = self.view;
  }
  return containerView;
}

#pragma mark - Setters

// Sets whether or not the NTP is scrolled into the feed and notifies the
// content suggestions layout to avoid it changing the omnibox frame when this
// view controls its position.
- (void)setIsScrolledIntoFeed:(BOOL)scrolledIntoFeed {
  _scrolledIntoFeed = scrolledIntoFeed;
  self.contentSuggestionsLayout.isScrolledIntoFeed = scrolledIntoFeed;
}

// Sets the feed collection contentOffset to |offset| to set the initial scroll
// position.
- (void)setContentOffset:(CGFloat)offset {
  self.collectionView.contentOffset = CGPointMake(0, offset);
  self.scrolledIntoFeed = offset > -[self offsetToStickOmniboxAndHeader];
}

@end
