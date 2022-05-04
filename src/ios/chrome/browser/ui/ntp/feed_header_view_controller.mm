// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/feed_header_view_controller.h"

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/feed_control_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Leading margin for title label. Its used to align with the Card leading
// margin.
const CGFloat kTitleHorizontalMargin = 19;
// Trailing/leading margins for header buttons. Its used to align with the card
// margins.
const CGFloat kButtonHorizontalMargin = 14;
// Font size for label text in header.
const CGFloat kDiscoverFeedTitleFontSize = 16;
// Font size for the custom search engine label.
const CGFloat kCustomSearchEngineLabelFontSize = 13;
// Insets for header menu button.
const CGFloat kHeaderMenuButtonInsetTopAndBottom = 2;
const CGFloat kHeaderMenuButtonInsetSides = 2;
// The width of the feed content. Currently hard coded in Mulder.
// TODO(crbug.com/1085419): Get card width from Mulder.
const CGFloat kDiscoverFeedContentWith = 430;
// The height of the header container. The content is unaffected.
// TODO(crbug.com/1277504): Only keep the WC header after launch.
const CGFloat kWebChannelsHeaderHeight = 52;
const CGFloat kDiscoverFeedHeaderHeight = 40;
const CGFloat kCustomSearchEngineLabelHeight = 18;
// * Values below are exclusive to Web Channels.
// The width of the segmented control to toggle between feeds.
// TODO(crbug.com/1277974): See how segments react to longer words.
const CGFloat kHeaderSegmentWidth = 300;
// The height and width of the header menu button. Based on the default
// UISegmentedControl height.
const CGFloat kButtonSize = 28;
// The radius of the dot indicating that there is new content in the Following
// feed.
const CGFloat kFollowingSegmentDotRadius = 3;
// The distance between the Following segment dot and the Following label.
const CGFloat kFollowingSegmentDotMargin = 8;
// Duration of the fade animation for elements that toggle when switching feeds.
const CGFloat kSegmentAnimationDuration = 0.3;

// Image names for feed header icons.
NSString* kMenuIcon = @"ellipsis";
NSString* kSortIcon = @"arrow.up.arrow.down";
// TODO(crbug.com/1277974): Remove this when Web Channels is launched.
NSString* kDiscoverMenuIcon = @"infobar_settings_icon";
}

@interface FeedHeaderViewController ()

// View containing elements of the header. Handles header sizing.
@property(nonatomic, strong) UIView* container;

// Title label element for the feed.
@property(nonatomic, strong) UILabel* titleLabel;

// Button for opening top-level feed menu.
// Redefined to not be readonly.
@property(nonatomic, strong) UIButton* menuButton;

// Button for sorting feed content. Only used for Following feed.
@property(nonatomic, strong) UIButton* sortButton;

// Segmented control for toggling between the two feeds.
@property(nonatomic, strong) UISegmentedControl* segmentedControl;

// The dot in the Following segment indicating new content in the Following
// feed.
@property(nonatomic, strong) UIView* followingSegmentDot;

// Currently selected feed.
@property(nonatomic, assign) FeedType selectedFeed;

// The blurred background of the feed header.
@property(nonatomic, strong) UIVisualEffectView* blurBackgroundView;

// The view informing the user that the feed is powered by Google if they don't
// have Google as their default search engine.
@property(nonatomic, strong) UILabel* customSearchEngineView;

// The constraints for the currently visible components of the header.
@property(nonatomic, strong)
    NSMutableArray<NSLayoutConstraint*>* feedHeaderConstraints;

@end

@implementation FeedHeaderViewController

- (instancetype)initWithSelectedFeed:(FeedType)selectedFeed
               followingFeedSortType:
                   (FollowingFeedSortType)followingFeedSortType
          followingSegmentDotVisible:(BOOL)followingSegmentDotVisible {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _selectedFeed = selectedFeed;
    _followingFeedSortType = followingFeedSortType;
    _followingSegmentDotVisible = followingSegmentDotVisible;

    // The menu button is created early so that it can be assigned a tap action
    // before the view loads.
    _menuButton = [[UIButton alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Applies an opacity to the background. If ReduceTransparency is enabled,
  // then this replaces the blur effect.
  self.view.backgroundColor =
      [[UIColor colorNamed:kBackgroundColor] colorWithAlphaComponent:0.95];

  self.container = [[UIView alloc] init];

  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  self.container.translatesAutoresizingMaskIntoConstraints = NO;

  [self configureMenuButton:self.menuButton];

  if (IsWebChannelsEnabled()) {
    self.segmentedControl = [self createSegmentedControl];
    [self.container addSubview:self.segmentedControl];

    self.sortButton = [self createSortButton];
    self.sortButton.menu = [self createSortMenu];
    [self.container addSubview:self.sortButton];

    self.followingSegmentDot = [self createFollowingSegmentDot];
    self.followingSegmentDot.hidden = !self.followingSegmentDotVisible;
    [self.segmentedControl addSubview:self.followingSegmentDot];

    if (!UIAccessibilityIsReduceTransparencyEnabled()) {
      self.blurBackgroundView = [self createBlurBackground];
      [self.view addSubview:self.blurBackgroundView];
      // The blurred background has a tint that is visible when the header is
      // over the standard NTP background. For this reason, we only add the blur
      // background when scrolled into the feed.
      self.blurBackgroundView.hidden = YES;
    }

    if (![self.ntpDelegate isGoogleDefaultSearchEngine]) {
      [self addCustomSearchEngineView];
    }
  } else {
    self.titleLabel = [self createTitleLabel];
    [self.container addSubview:self.titleLabel];
  }

  [self.container addSubview:self.menuButton];
  [self.view addSubview:self.container];
  [self applyHeaderConstraints];
}

- (void)toggleBackgroundBlur:(BOOL)blurred animated:(BOOL)animated {
  if (UIAccessibilityIsReduceTransparencyEnabled() || !IsWebChannelsEnabled()) {
    return;
  }
  DCHECK(self.blurBackgroundView);

  // Applies blur to header background. Also reduces opacity when blur is
  // applied so that the blur is still transluscent.
  if (!animated) {
    self.blurBackgroundView.hidden = !blurred;
    self.view.backgroundColor = [[UIColor colorNamed:kBackgroundColor]
        colorWithAlphaComponent:(blurred ? 0.1 : 0.95)];
    return;
  }
  [UIView transitionWithView:self.blurBackgroundView
      duration:0.3
      options:UIViewAnimationOptionTransitionCrossDissolve
      animations:^{
        self.blurBackgroundView.hidden = !blurred;
      }
      completion:^(BOOL finished) {
        // Only reduce opacity after the animation is complete to avoid showing
        // content suggestions tiles momentarily.
        self.view.backgroundColor = [[UIColor colorNamed:kBackgroundColor]
            colorWithAlphaComponent:(blurred ? 0.1 : 0.95)];
      }];
}

- (CGFloat)feedHeaderHeight {
  return IsWebChannelsEnabled() ? kWebChannelsHeaderHeight
                                : kDiscoverFeedHeaderHeight;
}

- (CGFloat)customSearchEngineViewHeight {
  return
      [self.ntpDelegate isGoogleDefaultSearchEngine] || !IsWebChannelsEnabled()
          ? 0
          : kCustomSearchEngineLabelHeight;
}

- (void)updateForDefaultSearchEngineChanged {
  DCHECK(IsWebChannelsEnabled());
  if ([self.ntpDelegate isGoogleDefaultSearchEngine]) {
    [self removeCustomSearchEngineView];
  } else {
    [self addCustomSearchEngineView];
  }
  [self applyHeaderConstraints];
}

#pragma mark - Setters

// Sets |titleText| and updates header label if it exists.
- (void)setTitleText:(NSString*)titleText {
  _titleText = titleText;
  if (self.titleLabel) {
    self.titleLabel.text = titleText;
    [self.titleLabel setNeedsDisplay];
  }
}

// Sets |followingFeedSortType| and recreates the sort menu to assign the active
// sort type.
- (void)setFollowingFeedSortType:(FollowingFeedSortType)followingFeedSortType {
  _followingFeedSortType = followingFeedSortType;
  if (self.sortButton) {
    self.sortButton.menu = [self createSortMenu];
  }
}

// Sets |followingSegmentDotVisible| and animates |followingSegmentDot|'s
// visibility accordingly.
- (void)setFollowingSegmentDotVisible:(BOOL)followingSegmentDotVisible {
  DCHECK(IsWebChannelsEnabled());
  _followingSegmentDotVisible = followingSegmentDotVisible;
  [UIView animateWithDuration:kSegmentAnimationDuration
                   animations:^{
                     self.followingSegmentDot.alpha =
                         followingSegmentDotVisible ? 1 : 0;
                   }];
}

#pragma mark - Private

// Creates sort menu with its content and active sort type.
- (UIMenu*)createSortMenu {
  NSMutableArray<UIAction*>* sortActions = [NSMutableArray array];

  // Create menu actions.
  UIAction* sortByPublisherAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_FEED_SORT_PUBLISHER)
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                [self.feedControlDelegate handleSortTypeForFollowingFeed:
                                              FollowingFeedSortTypeByPublisher];
              }];
  [sortActions addObject:sortByPublisherAction];
  UIAction* sortByLatestAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_FEED_SORT_LATEST)
                image:nil
           identifier:nil
              handler:^(UIAction* action) {
                [self.feedControlDelegate handleSortTypeForFollowingFeed:
                                              FollowingFeedSortTypeByLatest];
              }];
  [sortActions addObject:sortByLatestAction];

  // Set active sorting.
  switch (self.followingFeedSortType) {
    case FollowingFeedSortTypeByLatest:
      sortByLatestAction.state = UIMenuElementStateOn;
      break;
    case FollowingFeedSortTypeByPublisher:
    default:
      sortByPublisherAction.state = UIMenuElementStateOn;
  }

  return [UIMenu menuWithTitle:@"" children:sortActions];
}

// Configures the feed header's menu button.
- (void)configureMenuButton:(UIButton*)menuButton {
  menuButton.translatesAutoresizingMaskIntoConstraints = NO;
  menuButton.accessibilityIdentifier = kNTPFeedHeaderMenuButtonIdentifier;
  menuButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_MENU_ACCESSIBILITY_LABEL);
  if (IsWebChannelsEnabled()) {
    [menuButton setImage:[UIImage systemImageNamed:kMenuIcon]
                forState:UIControlStateNormal];
    menuButton.backgroundColor = [UIColor colorNamed:kGrey100Color];
    menuButton.clipsToBounds = YES;
    menuButton.layer.cornerRadius = kButtonSize / 2;
  } else {
    [menuButton
        setImage:[[UIImage imageNamed:kDiscoverMenuIcon]
                     imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
        forState:UIControlStateNormal];
    menuButton.tintColor = [UIColor colorNamed:kGrey600Color];
    menuButton.imageEdgeInsets = UIEdgeInsetsMake(
        kHeaderMenuButtonInsetTopAndBottom, kHeaderMenuButtonInsetSides,
        kHeaderMenuButtonInsetTopAndBottom, kHeaderMenuButtonInsetSides);
  }
}

// Configures and returns the feed header's sorting button.
- (UIButton*)createSortButton {
  DCHECK(IsWebChannelsEnabled());

  UIButton* sortButton = [[UIButton alloc] init];

  sortButton.translatesAutoresizingMaskIntoConstraints = NO;
  sortButton.accessibilityIdentifier = kNTPFeedHeaderSortButtonIdentifier;
  sortButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_FEED_SORT_ACCESSIBILITY_LABEL);
  [sortButton setImage:[UIImage systemImageNamed:kSortIcon]
              forState:UIControlStateNormal];
  sortButton.showsMenuAsPrimaryAction = YES;

  // The sort button is only visible if the Following feed is selected.
  // TODO(crbug.com/1277974): Determine if the button should show when the feed
  // is hidden.
  sortButton.alpha = self.selectedFeed == FeedTypeFollowing ? 1 : 0;

  return sortButton;
}

// Configures and returns the feed header's title label.
- (UILabel*)createTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont systemFontOfSize:kDiscoverFeedTitleFontSize
                                      weight:UIFontWeightMedium];
  titleLabel.textColor = [UIColor colorNamed:kGrey700Color];
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.accessibilityIdentifier =
      ntp_home::DiscoverHeaderTitleAccessibilityID();
  titleLabel.text = self.titleText;
  return titleLabel;
}

// Configures and returns the segmented control for toggling between feeds.
- (UISegmentedControl*)createSegmentedControl {
  // Create segmented control with labels.
  NSArray* headerLabels = [NSArray
      arrayWithObjects:l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE),
                       l10n_util::GetNSString(IDS_IOS_FOLLOWING_FEED_TITLE),
                       nil];
  UISegmentedControl* segmentedControl =
      [[UISegmentedControl alloc] initWithItems:headerLabels];
  segmentedControl.translatesAutoresizingMaskIntoConstraints = NO;
  [segmentedControl setApportionsSegmentWidthsByContent:NO];

  // Set text font and color.
  UIFont* font = [UIFont systemFontOfSize:kDiscoverFeedTitleFontSize
                                   weight:UIFontWeightMedium];
  NSDictionary* attributes =
      [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName];
  [segmentedControl setTitleTextAttributes:attributes
                                  forState:UIControlStateNormal];
  segmentedControl.backgroundColor = [UIColor colorNamed:kGrey100Color];

  // Set selected feed and tap action.
  segmentedControl.selectedSegmentIndex =
      static_cast<NSInteger>(self.selectedFeed);
  [segmentedControl addTarget:self
                       action:@selector(onSegmentSelected:)
             forControlEvents:UIControlEventValueChanged];

  return segmentedControl;
}

// Configures and returns the dot indicating that there is new content in the
// Following feed.
- (UIView*)createFollowingSegmentDot {
  UIView* followingSegmentDot = [[UIView alloc] init];
  followingSegmentDot.layer.cornerRadius = kFollowingSegmentDotRadius;
  followingSegmentDot.backgroundColor = [UIColor colorNamed:kBlue500Color];
  followingSegmentDot.translatesAutoresizingMaskIntoConstraints = NO;
  return followingSegmentDot;
}

// Configures and returns the blurred background of the feed header.
- (UIVisualEffectView*)createBlurBackground {
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial];
  UIVisualEffectView* blurBackgroundView =
      [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  blurBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  return blurBackgroundView;
}

- (void)addCustomSearchEngineView {
  if (self.customSearchEngineView) {
    [self removeCustomSearchEngineView];
  }
  self.customSearchEngineView = [[UILabel alloc] init];
  self.customSearchEngineView.text =
      l10n_util::GetNSString(IDS_IOS_FEED_CUSTOM_SEARCH_ENGINE_LABEL);
  self.customSearchEngineView.font =
      [UIFont systemFontOfSize:kCustomSearchEngineLabelFontSize];
  self.customSearchEngineView.textColor = [UIColor colorNamed:kGrey500Color];
  self.customSearchEngineView.translatesAutoresizingMaskIntoConstraints = NO;
  self.customSearchEngineView.textAlignment = NSTextAlignmentCenter;
  [self.view addSubview:self.customSearchEngineView];
}

- (void)removeCustomSearchEngineView {
  [self.customSearchEngineView removeFromSuperview];
  self.customSearchEngineView = nil;
}

// Applies constraints for the feed header elements' positioning.
- (void)applyHeaderConstraints {
  // Remove previous constraints if they were already set.
  if (self.feedHeaderConstraints) {
    [NSLayoutConstraint deactivateConstraints:self.feedHeaderConstraints];
    self.feedHeaderConstraints = nil;
  }

  self.feedHeaderConstraints = [[NSMutableArray alloc] init];

  [self.feedHeaderConstraints addObjectsFromArray:@[
    // Anchor container and menu button.
    [self.view.heightAnchor
        constraintEqualToConstant:([self feedHeaderHeight] +
                                   [self customSearchEngineViewHeight])],
    [self.container.heightAnchor
        constraintEqualToConstant:kWebChannelsHeaderHeight],
    [self.container.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.container.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.container.widthAnchor
        constraintEqualToConstant:MIN(kDiscoverFeedContentWith,
                                      self.view.frame.size.width)],
    [self.menuButton.trailingAnchor
        constraintEqualToAnchor:self.container.trailingAnchor
                       constant:-kButtonHorizontalMargin],
    [self.menuButton.centerYAnchor
        constraintEqualToAnchor:self.container.centerYAnchor],
  ]];

  if (IsWebChannelsEnabled()) {
    [self.feedHeaderConstraints addObjectsFromArray:@[
      // Anchor segmented control.
      [self.segmentedControl.centerXAnchor
          constraintEqualToAnchor:self.container.centerXAnchor],
      [self.segmentedControl.centerYAnchor
          constraintEqualToAnchor:self.container.centerYAnchor],
      [self.segmentedControl.trailingAnchor
          constraintEqualToAnchor:self.menuButton.leadingAnchor
                         constant:-kButtonHorizontalMargin],
      [self.segmentedControl.leadingAnchor
          constraintEqualToAnchor:self.sortButton.trailingAnchor
                         constant:kButtonHorizontalMargin],
      [self.segmentedControl.widthAnchor
          constraintLessThanOrEqualToConstant:kHeaderSegmentWidth],
      // Set menu button size.
      [self.menuButton.heightAnchor constraintEqualToConstant:kButtonSize],
      [self.menuButton.widthAnchor constraintEqualToConstant:kButtonSize],
      // Anchor sort button and set size.
      [self.sortButton.heightAnchor constraintEqualToConstant:kButtonSize],
      [self.sortButton.widthAnchor constraintEqualToConstant:kButtonSize],
      [self.sortButton.leadingAnchor
          constraintEqualToAnchor:self.container.leadingAnchor
                         constant:kButtonHorizontalMargin],
      [self.sortButton.centerYAnchor
          constraintEqualToAnchor:self.container.centerYAnchor],
      // Set Following segment dot size.
      [self.followingSegmentDot.heightAnchor
          constraintEqualToConstant:kFollowingSegmentDotRadius * 2],
      [self.followingSegmentDot.widthAnchor
          constraintEqualToConstant:kFollowingSegmentDotRadius * 2],
    ]];

    // Find the "Following" label within the segmented control, since it is not
    // exposed by UISegmentedControl.
    UIView* followingSegment = self.segmentedControl.subviews[0];
    UILabel* followingLabel;
    for (UIView* view in followingSegment.subviews) {
      if ([view isKindOfClass:[UILabel class]]) {
        followingLabel = static_cast<UILabel*>(view);
      }
    }

    // If the label was found, anchor the dot to it. Otherwise, anchor the dot
    // to the top corner of the segmented control.
    if (followingLabel) {
      [self.feedHeaderConstraints addObjectsFromArray:@[
        // Anchor Following segment dot to label text.
        [self.followingSegmentDot.leftAnchor
            constraintEqualToAnchor:followingLabel.rightAnchor
                           constant:kFollowingSegmentDotMargin],
        [self.followingSegmentDot.bottomAnchor
            constraintEqualToAnchor:followingLabel.topAnchor
                           constant:kFollowingSegmentDotMargin],
      ]];
    } else {
      [self.feedHeaderConstraints addObjectsFromArray:@[
        // Anchor Following segment dot to top corner.
        [self.followingSegmentDot.rightAnchor
            constraintEqualToAnchor:self.segmentedControl.rightAnchor
                           constant:-kFollowingSegmentDotMargin],
        [self.followingSegmentDot.topAnchor
            constraintEqualToAnchor:self.segmentedControl.topAnchor
                           constant:kFollowingSegmentDotMargin],
      ]];
    }

    if (self.blurBackgroundView) {
      [self.feedHeaderConstraints addObjectsFromArray:@[
        // Anchor blur background view.
        [self.blurBackgroundView.trailingAnchor
            constraintEqualToAnchor:self.container.trailingAnchor],
        [self.blurBackgroundView.leadingAnchor
            constraintEqualToAnchor:self.container.leadingAnchor],
        [self.blurBackgroundView.topAnchor
            constraintEqualToAnchor:self.container.topAnchor],
        [self.blurBackgroundView.bottomAnchor
            constraintEqualToAnchor:self.container.bottomAnchor],
      ]];
    }
    if (![self.ntpDelegate isGoogleDefaultSearchEngine]) {
      [self.feedHeaderConstraints addObjectsFromArray:@[
        // Anchors custom search engine view.
        [self.customSearchEngineView.widthAnchor
            constraintEqualToAnchor:self.view.widthAnchor],
        [self.customSearchEngineView.heightAnchor
            constraintEqualToConstant:kCustomSearchEngineLabelHeight],
        [self.customSearchEngineView.bottomAnchor
            constraintEqualToAnchor:self.container.topAnchor],
      ]];
    }
  } else {
    [self.feedHeaderConstraints addObjectsFromArray:@[
      // Anchors title label.
      [self.view.heightAnchor
          constraintEqualToConstant:kDiscoverFeedHeaderHeight],
      [self.titleLabel.leadingAnchor
          constraintEqualToAnchor:self.container.leadingAnchor
                         constant:kTitleHorizontalMargin],
      [self.titleLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.menuButton.leadingAnchor],
      [self.titleLabel.centerYAnchor
          constraintEqualToAnchor:self.container.centerYAnchor],
    ]];
  }
  [NSLayoutConstraint activateConstraints:self.feedHeaderConstraints];
}

// Handles a new feed being selected from the header.
- (void)onSegmentSelected:(UISegmentedControl*)segmentedControl {
  switch (segmentedControl.selectedSegmentIndex) {
    case static_cast<NSInteger>(FeedTypeDiscover): {
      [self.feedControlDelegate handleFeedSelected:FeedTypeDiscover];
      [UIView animateWithDuration:kSegmentAnimationDuration
                       animations:^{
                         self.sortButton.alpha = 0;
                       }];
      break;
    }
    case static_cast<NSInteger>(FeedTypeFollowing): {
      [self.feedControlDelegate handleFeedSelected:FeedTypeFollowing];
      // Only show sorting button for Following feed.
      [UIView animateWithDuration:kSegmentAnimationDuration
                       animations:^{
                         self.sortButton.alpha = 1;
                       }];
      break;
    }
  }
}

@end
