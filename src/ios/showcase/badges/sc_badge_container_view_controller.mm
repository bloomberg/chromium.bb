// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/badges/sc_badge_container_view_controller.h"

#import "ios/chrome/browser/infobars/badge_state.h"
#import "ios/chrome/browser/ui/badges/badge_button.h"
#import "ios/chrome/browser/ui/badges/badge_button_factory.h"
#import "ios/chrome/browser/ui/badges/badge_overflow_menu_util.h"
#import "ios/chrome/browser/ui/badges/badge_static_item.h"
#import "ios/chrome/browser/ui/badges/badge_tappable_item.h"
#import "ios/chrome/browser/ui/badges/badge_type.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/named_guide_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/showcase/badges/sc_badge_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewPopupMenuBadgeButtonFactory : BadgeButtonFactory
@end

@implementation NewPopupMenuBadgeButtonFactory

- (BadgeButton*)overflowBadgeButton {
  BadgeButton* button = [BadgeButton badgeButtonWithType:kBadgeTypeOverflow];
  button.image = [[UIImage imageNamed:@"wrench_badge"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  button.fullScreenOn = NO;
  [button addTarget:self.delegate
                action:@selector(overflowBadgeButtonTapped:)
      forControlEvents:UIControlEventMenuActionTriggered];
  [NSLayoutConstraint
      activateConstraints:@[ [button.widthAnchor
                              constraintEqualToAnchor:button.heightAnchor] ]];
  button.imageView.contentMode = UIViewContentModeScaleAspectFit;
  button.showsMenuAsPrimaryAction = YES;
  button.menu = GetOverflowMenuFromBadgeTypes(
      self.delegate.badgeTypesForOverflowMenu, ^(BadgeType badgeType){
      });

  return button;
}

@end

@interface SCBadgeContainerViewController ()

@property(nonatomic, weak) id<BadgeDelegate> badgeDelegate;
@property(nonatomic, strong) BadgeViewController* badgeViewController;
@property(nonatomic, strong) UISwitch* overflowPopupSwitch;

@end

@implementation SCBadgeContainerViewController

- (instancetype)initWithBadgeDelegate:(id<BadgeDelegate>)delegate {
  self = [super init];
  if (self) {
    self.badgeDelegate = delegate;
    BadgeButtonFactory* factory = [[BadgeButtonFactory alloc] init];
    factory.delegate = self.badgeDelegate;
    self.badgeViewController =
        [[BadgeViewController alloc] initWithButtonFactory:factory];
    self.consumer = self.badgeViewController;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  UIStackView* containerStack = [[UIStackView alloc] init];
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  containerStack.axis = UILayoutConstraintAxisVertical;

  [self setupBadgeViewController];
  [containerStack addArrangedSubview:self.badgeViewController.view];

  UIStackView* buttonStackView = [[UIStackView alloc] init];
  buttonStackView.axis = UILayoutConstraintAxisHorizontal;
  UIButton* showAcceptedBadgeButton =
      [UIButton buttonWithType:UIButtonTypeSystem];
  showAcceptedBadgeButton.accessibilityIdentifier =
      kSCShowAcceptedDisplayedBadgeButton;
  showAcceptedBadgeButton.titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
  showAcceptedBadgeButton.titleLabel.textAlignment = NSTextAlignmentCenter;
  [showAcceptedBadgeButton setTitle:@"Show Accepted Badge"
                           forState:UIControlStateNormal];
  [showAcceptedBadgeButton addTarget:self
                              action:@selector(showAcceptedDisplayedBadge)
                    forControlEvents:UIControlEventTouchUpInside];
  [buttonStackView addArrangedSubview:showAcceptedBadgeButton];

  UIButton* showOverflowBadgeButton =
      [UIButton buttonWithType:UIButtonTypeSystem];
  showOverflowBadgeButton.accessibilityIdentifier =
      kSCShowOverflowDisplayedBadgeButton;
  showOverflowBadgeButton.titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
  showOverflowBadgeButton.titleLabel.textAlignment = NSTextAlignmentCenter;
  [showOverflowBadgeButton setTitle:@"Show Overflow badge"
                           forState:UIControlStateNormal];
  [showOverflowBadgeButton addTarget:self
                              action:@selector(addSecondBadge:)
                    forControlEvents:UIControlEventTouchUpInside];
  [buttonStackView addArrangedSubview:showOverflowBadgeButton];

  UILabel* overflowSwitchLabel = [[UILabel alloc] init];
  overflowSwitchLabel.text = @"Show new popup menu";
  self.overflowPopupSwitch = [[UISwitch alloc] init];
  [self.overflowPopupSwitch addTarget:self
                               action:@selector(updateOverflowBadge)
                     forControlEvents:UIControlEventValueChanged];
  UIStackView* overflowStackView = [[UIStackView alloc] init];
  buttonStackView.axis = UILayoutConstraintAxisHorizontal;
  [overflowStackView addArrangedSubview:overflowSwitchLabel];
  [overflowStackView addArrangedSubview:self.overflowPopupSwitch];

  [containerStack addArrangedSubview:buttonStackView];
  [containerStack addArrangedSubview:overflowStackView];
  containerStack.spacing = 25;
  [self.view addSubview:containerStack];
  AddSameCenterConstraints(containerStack, self.view);
  [NSLayoutConstraint activateConstraints:@[
    [containerStack.widthAnchor constraintEqualToConstant:300],
    [containerStack.heightAnchor constraintGreaterThanOrEqualToConstant:100]
  ]];

  UIView* containerView = self.view;
  containerView.backgroundColor = [UIColor whiteColor];
  self.title = @"Badges";
}

#pragma mark - Accessor

- (BOOL)useNewPopupUI {
  return self.overflowPopupSwitch != nil && self.overflowPopupSwitch.on;
}

#pragma mark - Helpers

- (void)showAcceptedDisplayedBadge {
  BadgeStaticItem* incognitoItem =
      [[BadgeStaticItem alloc] initWithBadgeType:kBadgeTypeIncognito];
  BadgeTappableItem* passwordBadgeItem =
      [[BadgeTappableItem alloc] initWithBadgeType:kBadgeTypePasswordSave];
  passwordBadgeItem.badgeState = BadgeStateRead | BadgeStateAccepted;
  [self.consumer setupWithDisplayedBadge:passwordBadgeItem
                         fullScreenBadge:incognitoItem];
}

- (void)addSecondBadge:(id)sender {
  BadgeStaticItem* incognitoItem =
      [[BadgeStaticItem alloc] initWithBadgeType:kBadgeTypeIncognito];
  BadgeTappableItem* displayedBadge =
      [[BadgeTappableItem alloc] initWithBadgeType:kBadgeTypeOverflow];
  [self.consumer setupWithDisplayedBadge:displayedBadge
                         fullScreenBadge:incognitoItem];
  [self.consumer markDisplayedBadgeAsRead:NO];
}

- (void)updateOverflowBadge {
  UIStackView* containerStack =
      (UIStackView*)[self.badgeViewController.view superview];
  [self.badgeViewController removeFromParentViewController];
  [self.badgeViewController.view removeFromSuperview];

  [self setupBadgeViewController];
  [containerStack insertArrangedSubview:self.badgeViewController.view
                                atIndex:0];
}

- (void)setupBadgeViewController {
  BadgeButtonFactory* factory;
  if (self.useNewPopupUI) {
    factory = [[NewPopupMenuBadgeButtonFactory alloc] init];
  } else {
    factory = [[BadgeButtonFactory alloc] init];
  }
  factory.delegate = self.badgeDelegate;

  self.badgeViewController =
      [[BadgeViewController alloc] initWithButtonFactory:factory];
  [self addChildViewController:self.badgeViewController];
  [self didMoveToParentViewController:self.badgeViewController];
  self.consumer = self.badgeViewController;

  AddNamedGuidesToView(@[ kBadgeOverflowMenuGuide ], self.view);
  BadgeStaticItem* incognitoItem =
      [[BadgeStaticItem alloc] initWithBadgeType:kBadgeTypeIncognito];
  BadgeTappableItem* passwordBadgeItem =
      [[BadgeTappableItem alloc] initWithBadgeType:kBadgeTypePasswordSave];
  passwordBadgeItem.badgeState = BadgeStateRead;
  [self.consumer setupWithDisplayedBadge:passwordBadgeItem
                         fullScreenBadge:incognitoItem];
}

@end
