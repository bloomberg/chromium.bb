// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_parent_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_selection_actions.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_shortcut_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_layout_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_whats_new_view.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_menu_provider.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionsViewController () <
    UIGestureRecognizerDelegate,
    ContentSuggestionsSelectionActions>

// Whether an item of type ItemTypePromo has already been added to the model.
@property(nonatomic, assign) BOOL promoAdded;

// StackView holding all subviews.
@property(nonatomic, strong) UIStackView* verticalStackView;

// List of all UITapGestureRecognizers created for the Most Visisted tiles.
@property(nonatomic, strong)
    NSMutableArray<UITapGestureRecognizer*>* mostVisitedTapRecognizers;
// The UITapGestureRecognizer for the Return To Recent Tab tile.
@property(nonatomic, strong)
    UITapGestureRecognizer* returnToRecentTabTapRecognizer;
// The UITapGestureRecognizer for the NTP promo view.
@property(nonatomic, strong) UITapGestureRecognizer* promoTapRecognizer;

// The Return To Recent Tab view.
@property(nonatomic, strong)
    ContentSuggestionsReturnToRecentTabView* returnToRecentTabTile;
// The WhatsNew view.
@property(nonatomic, strong) ContentSuggestionsWhatsNewView* whatsNewView;
// StackView holding all of |mostVisitedViews|.
@property(nonatomic, strong) UIStackView* mostVisitedStackView;
// List of all of the Most Visited views.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsMostVisitedTileView*>* mostVisitedViews;
// StackView holding all of |shortcutsViews|.
@property(nonatomic, strong) UIStackView* shortcutsStackView;
// List of all of the Shortcut views.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsShortcutTileView*>* shortcutsViews;

@end

@implementation ContentSuggestionsViewController

- (instancetype)init {
  return [super initWithNibName:nil bundle:nil];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = ntp_home::kNTPBackgroundColor();
  self.view.accessibilityIdentifier = kContentSuggestionsCollectionIdentifier;

  self.verticalStackView = [[UIStackView alloc] init];
  self.verticalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.verticalStackView.axis = UILayoutConstraintAxisVertical;
  // A centered alignment will ensure the views are centered.
  self.verticalStackView.alignment = UIStackViewAlignmentCenter;
  // A fill distribution allows for the custom spacing between elements and
  // height/width configurations for each row.
  self.verticalStackView.distribution = UIStackViewDistributionFill;
  [self.view addSubview:_verticalStackView];
  AddSameConstraints(self.view, _verticalStackView);

  CGFloat horizontalSpacing =
      ContentSuggestionsTilesHorizontalSpacing(self.traitCollection);
  if (self.returnToRecentTabTile) {
    [self addUIElement:self.returnToRecentTabTile
        withCustomBottomSpacing:content_suggestions::
                                    kReturnToRecentTabSectionBottomMargin];
    CGFloat cardWidth = content_suggestions::searchFieldWidth(
        self.view.bounds.size.width, self.traitCollection);
    [NSLayoutConstraint activateConstraints:@[
      [self.returnToRecentTabTile.widthAnchor
          constraintEqualToConstant:cardWidth],
      [self.returnToRecentTabTile.heightAnchor
          constraintEqualToConstant:kReturnToRecentTabSize.height]
    ]];
  }
  if (self.whatsNewView) {
    [self addUIElement:self.whatsNewView withCustomBottomSpacing:0];
    CGFloat width =
        MostVisitedTilesContentHorizontalSpace(self.traitCollection);
    CGSize size =
        MostVisitedCellSize(self.traitCollection.preferredContentSizeCategory);
    [NSLayoutConstraint activateConstraints:@[
      [self.whatsNewView.widthAnchor constraintEqualToConstant:width],
      [self.whatsNewView.heightAnchor constraintEqualToConstant:size.height]
    ]];
  }
  if (self.mostVisitedViews) {
    self.mostVisitedStackView = [[UIStackView alloc] init];
    self.mostVisitedStackView.axis = UILayoutConstraintAxisHorizontal;
    self.mostVisitedStackView.alignment = UIStackViewAlignmentTop;
    self.mostVisitedStackView.distribution = UIStackViewDistributionFillEqually;
    self.mostVisitedStackView.spacing = horizontalSpacing;
    [self addUIElement:self.mostVisitedStackView
        withCustomBottomSpacing:kMostVisitedBottomMargin];
    CGFloat width =
        MostVisitedTilesContentHorizontalSpace(self.traitCollection);
    CGSize size =
        MostVisitedCellSize(self.traitCollection.preferredContentSizeCategory);
    [NSLayoutConstraint activateConstraints:@[
      [self.mostVisitedStackView.widthAnchor constraintEqualToConstant:width],
      [self.mostVisitedStackView.heightAnchor
          constraintEqualToConstant:size.height]
    ]];
    [self populateMostVisitedModule];
  }
  if (self.shortcutsViews) {
    self.shortcutsStackView = [[UIStackView alloc] init];
    self.shortcutsStackView.axis = UILayoutConstraintAxisHorizontal;
    self.shortcutsStackView.alignment = UIStackViewAlignmentTop;
    self.shortcutsStackView.distribution = UIStackViewDistributionFillEqually;
    self.shortcutsStackView.spacing = horizontalSpacing;
    NSUInteger index = 0;
    for (ContentSuggestionsShortcutTileView* view in self.shortcutsViews) {
      view.accessibilityIdentifier = [NSString
          stringWithFormat:
              @"%@%li",
              kContentSuggestionsShortcutsAccessibilityIdentifierPrefix, index];
      UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(contentSuggestionsElementTapped:)];
      [view addGestureRecognizer:tapRecognizer];
      [self.mostVisitedTapRecognizers addObject:tapRecognizer];
      [self.shortcutsStackView addArrangedSubview:view];
      index++;
    }

    [self addUIElement:self.shortcutsStackView
        withCustomBottomSpacing:kMostVisitedBottomMargin];
    CGFloat width =
        MostVisitedTilesContentHorizontalSpace(self.traitCollection);
    CGSize size =
        MostVisitedCellSize(self.traitCollection.preferredContentSizeCategory);
    [NSLayoutConstraint activateConstraints:@[
      [self.shortcutsStackView.widthAnchor constraintEqualToConstant:width],
      [self.shortcutsStackView.heightAnchor
          constraintEqualToConstant:size.height]
    ]];
  }
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  if (ShouldShowReturnToMostRecentTabForStartSurface()) {
    [self.audience viewDidDisappear];
  }
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  return touch.view.accessibilityIdentifier !=
             ntp_home::FakeOmniboxAccessibilityID() &&
         touch.view.superview.accessibilityIdentifier !=
             ntp_home::FakeOmniboxAccessibilityID();
}

#pragma mark - ContentSuggestionsConsumer

- (void)showReturnToRecentTabTileWithConfig:
    (ContentSuggestionsReturnToRecentTabItem*)config {
  self.returnToRecentTabTile = [[ContentSuggestionsReturnToRecentTabView alloc]
      initWithConfiguration:config];
  self.returnToRecentTabTapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(contentSuggestionsElementTapped:)];
  [self.returnToRecentTabTile
      addGestureRecognizer:self.returnToRecentTabTapRecognizer];
  self.returnToRecentTabTapRecognizer.enabled = YES;
  // If the Content Suggestions is already shown, add the Return to Recent Tab
  // tile to the StackView.
  if ([[self.verticalStackView arrangedSubviews] count]) {
    [self.verticalStackView insertArrangedSubview:self.returnToRecentTabTile
                                          atIndex:0];
    [self.verticalStackView
        setCustomSpacing:content_suggestions::
                             kReturnToRecentTabSectionBottomMargin
               afterView:self.returnToRecentTabTile];
    CGFloat cardWidth = content_suggestions::searchFieldWidth(
        self.view.bounds.size.width, self.traitCollection);
    [NSLayoutConstraint activateConstraints:@[
      [self.returnToRecentTabTile.widthAnchor
          constraintEqualToConstant:cardWidth],
      [self.returnToRecentTabTile.heightAnchor
          constraintEqualToConstant:kReturnToRecentTabSize.height]
    ]];
    [self.audience returnToRecentTabWasAdded];
  }
}

- (void)updateReturnToRecentTabTileWithConfig:
    (ContentSuggestionsReturnToRecentTabItem*)config {
  if (config.icon) {
    self.returnToRecentTabTile.iconImageView.image = config.icon;
    self.returnToRecentTabTile.iconImageView.hidden = NO;
  }
}

- (void)hideReturnToRecentTabTile {
  [self.returnToRecentTabTile removeFromSuperview];
  self.returnToRecentTabTile = nil;
}

- (void)showWhatsNewViewWithConfig:(ContentSuggestionsWhatsNewItem*)config {
  self.whatsNewView =
      [[ContentSuggestionsWhatsNewView alloc] initWithConfiguration:config];
  self.promoTapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(contentSuggestionsElementTapped:)];
  [self.whatsNewView addGestureRecognizer:self.promoTapRecognizer];
  self.promoTapRecognizer.enabled = YES;
}

- (void)hideWhatsNewView {
  [self.whatsNewView removeFromSuperview];
  self.whatsNewView = nil;
}

- (void)setMostVisitedTilesWithConfigs:
    (NSArray<ContentSuggestionsMostVisitedItem*>*)configs {
  if ([self.mostVisitedViews count]) {
    for (ContentSuggestionsMostVisitedTileView* view in self.mostVisitedViews) {
      [view removeFromSuperview];
    }
    [self.mostVisitedViews removeAllObjects];
    [self.mostVisitedTapRecognizers removeAllObjects];
  } else {
    self.mostVisitedViews = [NSMutableArray array];
  }
  NSInteger index = 0;
  for (ContentSuggestionsMostVisitedItem* item in configs) {
    ContentSuggestionsMostVisitedTileView* view =
        [[ContentSuggestionsMostVisitedTileView alloc]
            initWithConfiguration:item];
    view.menuProvider = self.menuProvider;
    view.accessibilityIdentifier = [NSString
        stringWithFormat:
            @"%@%li",
            kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix, index];
    [self.mostVisitedViews addObject:view];
  }
  [self populateMostVisitedModule];
}

- (void)setShortcutTilesWithConfigs:
    (NSArray<ContentSuggestionsMostVisitedActionItem*>*)configs {
  if (!self.shortcutsViews) {
    self.shortcutsViews = [NSMutableArray array];
  }
  // Assumes this only called before viewDidLoad, so there is no need to add the
  // views into the view hierarchy here.
  for (ContentSuggestionsMostVisitedActionItem* item in configs) {
    ContentSuggestionsShortcutTileView* view =
        [[ContentSuggestionsShortcutTileView alloc] initWithConfiguration:item];
    [self.shortcutsViews addObject:view];
  }
}

- (void)updateReadingListCount:(NSInteger)count {
  for (ContentSuggestionsShortcutTileView* view in self.shortcutsViews) {
    if (view.config.collectionShortcutType ==
        NTPCollectionShortcutTypeReadingList) {
      [view updateCount:count];
      return;
    }
  }
}

- (void)updateMostVisitedTileConfig:(ContentSuggestionsMostVisitedItem*)config {
  for (ContentSuggestionsMostVisitedTileView* view in self.mostVisitedViews) {
    if (view.config == config) {
      dispatch_async(dispatch_get_main_queue(), ^{
        [view.faviconView configureWithAttributes:config.attributes];
      });
      return;
    }
  }
}

- (CGFloat)contentSuggestionsHeight {
  CGFloat height = 0;
  if ([self.mostVisitedViews count] > 0) {
    height += MostVisitedCellSize(
                  UIApplication.sharedApplication.preferredContentSizeCategory)
                  .height +
              kMostVisitedBottomMargin;
  }
  if ([self.shortcutsViews count] > 0) {
    height += MostVisitedCellSize(
                  UIApplication.sharedApplication.preferredContentSizeCategory)
                  .height +
              kMostVisitedBottomMargin;
  }
  if (self.returnToRecentTabTile) {
    height += (kReturnToRecentTabSize.height +
               content_suggestions::kReturnToRecentTabSectionBottomMargin);
  }
  if (self.whatsNewView) {
    height += MostVisitedCellSize(
                  UIApplication.sharedApplication.preferredContentSizeCategory)
                  .height;
  }
  return height;
}

#pragma mark - ContentSuggestionsSelectionActions

- (void)contentSuggestionsElementTapped:(UIGestureRecognizer*)sender {
  if ([sender.view
          isKindOfClass:[ContentSuggestionsMostVisitedTileView class]]) {
    ContentSuggestionsMostVisitedTileView* mostVisitedView =
        static_cast<ContentSuggestionsMostVisitedTileView*>(sender.view);
    [self.suggestionCommandHandler
        openMostVisitedItem:mostVisitedView.config
                    atIndex:mostVisitedView.config.index];
  } else if ([sender.view
                 isKindOfClass:[ContentSuggestionsShortcutTileView class]]) {
    ContentSuggestionsShortcutTileView* shortcutView =
        static_cast<ContentSuggestionsShortcutTileView*>(sender.view);
    int index = static_cast<int>(shortcutView.config.index);
    [self.suggestionCommandHandler openMostVisitedItem:shortcutView.config
                                               atIndex:index];
  } else if ([sender.view isKindOfClass:[ContentSuggestionsReturnToRecentTabView
                                            class]]) {
    ContentSuggestionsReturnToRecentTabView* returnToRecentTabView =
        static_cast<ContentSuggestionsReturnToRecentTabView*>(sender.view);
    __weak ContentSuggestionsReturnToRecentTabView* weakRecentTabView =
        returnToRecentTabView;
    UIGestureRecognizerState state = sender.state;
    if (state == UIGestureRecognizerStateChanged ||
        state == UIGestureRecognizerStateCancelled) {
      // Do nothing if isn't a gesture start or end.
      // If the gesture was cancelled by the system, then reset the background
      // color since UIGestureRecognizerStateEnded will not be received.
      if (state == UIGestureRecognizerStateCancelled) {
        returnToRecentTabView.backgroundColor = [UIColor clearColor];
      }
      return;
    }
    BOOL touchBegan = state == UIGestureRecognizerStateBegan;
    [UIView transitionWithView:returnToRecentTabView
                      duration:ios::material::kDuration8
                       options:UIViewAnimationOptionCurveEaseInOut
                    animations:^{
                      weakRecentTabView.backgroundColor =
                          touchBegan ? [UIColor colorNamed:kGrey100Color]
                                     : [UIColor clearColor];
                    }
                    completion:nil];
    if (state == UIGestureRecognizerStateEnded) {
      CGPoint point = [sender locationInView:returnToRecentTabView];
      if (point.x < 0 || point.y < 0 ||
          point.x > kReturnToRecentTabSize.width ||
          point.y > kReturnToRecentTabSize.height) {
        // Reset the highlighted state and do nothing if the gesture ended
        // outside of the tile.
        returnToRecentTabView.backgroundColor = [UIColor clearColor];
        return;
      }
      [self.suggestionCommandHandler openMostRecentTab];
    }
  } else if ([sender.view
                 isKindOfClass:[ContentSuggestionsWhatsNewView class]]) {
    [self.suggestionCommandHandler handlePromoTapped];
  }
}

#pragma mark - Private

- (void)addUIElement:(UIView*)view withCustomBottomSpacing:(CGFloat)spacing {
  [self.verticalStackView addArrangedSubview:view];
  if (spacing > 0) {
    [self.verticalStackView setCustomSpacing:spacing afterView:view];
  }
}

// Add the elements in |mostVisitedViews| into |verticalStackView|, constructing
// |verticalStackView| beforehand if it has not been yet.
- (void)populateMostVisitedModule {
  // If viewDidLoad has been called before the first valid Most Visited Tiles
  // are available, construct |mostVisitedStackView|.
  if (self.verticalStackView && !self.mostVisitedStackView) {
    self.mostVisitedStackView = [[UIStackView alloc] init];
    self.mostVisitedStackView.axis = UILayoutConstraintAxisHorizontal;
    self.mostVisitedStackView.alignment = UIStackViewAlignmentTop;
    self.mostVisitedStackView.distribution = UIStackViewDistributionFillEqually;
    self.mostVisitedStackView.spacing =
        ContentSuggestionsTilesHorizontalSpacing(self.traitCollection);
    // Find correct insertion position in the stack.
    int insertionIndex = 0;
    if (self.returnToRecentTabTile) {
      insertionIndex++;
    }
    if (self.whatsNewView) {
      insertionIndex++;
    }
    [self.verticalStackView insertArrangedSubview:self.mostVisitedStackView
                                          atIndex:insertionIndex];
    [self.verticalStackView setCustomSpacing:kMostVisitedBottomMargin
                                   afterView:self.mostVisitedStackView];
    CGFloat width =
        MostVisitedTilesContentHorizontalSpace(self.traitCollection);
    CGSize size =
        MostVisitedCellSize(self.traitCollection.preferredContentSizeCategory);
    [NSLayoutConstraint activateConstraints:@[
      [self.mostVisitedStackView.widthAnchor constraintEqualToConstant:width],
      [self.mostVisitedStackView.heightAnchor
          constraintEqualToConstant:size.height]
    ]];
  }
  for (ContentSuggestionsMostVisitedTileView* view in self.mostVisitedViews) {
    view.menuProvider = self.menuProvider;
    UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(contentSuggestionsElementTapped:)];
    [view addGestureRecognizer:tapRecognizer];
    tapRecognizer.enabled = YES;
    [self.mostVisitedTapRecognizers addObject:tapRecognizer];
    [self.mostVisitedStackView addArrangedSubview:view];
  }
}

@end
