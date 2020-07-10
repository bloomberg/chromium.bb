// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Banner View constants.
const CGFloat kBannerViewCornerRadius = 13.0;
const CGFloat kBannerViewYShadowOffset = 3.0;
const CGFloat kBannerViewShadowRadius = 9.0;
const CGFloat kBannerViewShadowOpacity = 0.23;

// Banner View selected constants.
const CGFloat kTappedBannerViewScale = 0.98;
const CGFloat kSelectedBannerViewScale = 1.02;
const CGFloat kSelectBannerAnimationDurationInSeconds = 0.2;
const CGFloat kTappedBannerAnimationDurationInSeconds = 0.05;
const CGFloat kSelectedBannerViewYShadowOffset = 8.0;

// Button constants.
const CGFloat kButtonWidth = 100.0;
const CGFloat kButtonSeparatorWidth = 1.0;
const CGFloat kButtonMaxFontSize = 45;

// Container Stack constants.
const CGFloat kContainerStackSpacing = 10.0;
const CGFloat kContainerStackVerticalPadding = 18.0;
const CGFloat kContainerStackHorizontalPadding = 15.0;

// Icon constants.
const CGFloat kIconWidth = 28.0;
const CGFloat kIconHeight = 28.0;
const CGFloat kIconCornerRadius = 5.0;

// Gesture constants.
const CGFloat kChangeInPositionForDismissal = -15.0;
const CGFloat kLongPressTimeDurationInSeconds = 0.4;
}  // namespace

@interface InfobarBannerViewController ()

// The original position of this InfobarVC view in the parent's view coordinate
// system.
@property(nonatomic, assign) CGPoint originalCenter;
// The starting point of the LongPressGesture, used to calculate the gesture
// translation.
@property(nonatomic, assign) CGPoint startingTouch;
// Delegate to handle this InfobarVC actions.
@property(nonatomic, weak) id<InfobarBannerDelegate> delegate;
// YES if the user is interacting with the view via a touch gesture.
@property(nonatomic, assign) BOOL touchInProgress;
// YES if the view should be dismissed after any touch gesture has ended.
@property(nonatomic, assign) BOOL shouldDismissAfterTouchesEnded;
// UIButton which opens the modal.
@property(nonatomic, strong) UIButton* openModalButton;
// UIButton with title |self.buttonText|, which triggers the Infobar action.
@property(nonatomic, strong) UIButton* infobarButton;
// UILabel displaying |self.titleText|.
@property(nonatomic, strong) UILabel* titleLabel;
// UILabel displaying |self.subTitleText|.
@property(nonatomic, strong) UILabel* subTitleLabel;
// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;
// The NSTimeInterval in which the Banner appeared on screen.
@property(nonatomic, assign) NSTimeInterval bannerAppearedTime;
// YES if the banner on screen time metric has already been recorded for this
// banner.
@property(nonatomic, assign) BOOL bannerOnScreenTimeWasRecorded;

@end

@implementation InfobarBannerViewController
@synthesize interactionDelegate = _interactionDelegate;

- (instancetype)initWithDelegate:(id<InfobarBannerDelegate>)delegate
                   presentsModal:(BOOL)presentsModal
                            type:(InfobarType)infobarType {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _delegate = delegate;
    _metricsRecorder =
        [[InfobarMetricsRecorder alloc] initWithType:infobarType];
    _presentsModal = presentsModal;
  }
  return self;
}

#pragma mark - View Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];

  // BannerView setup.
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.view.layer.cornerRadius = kBannerViewCornerRadius;
  [self.view.layer setShadowOffset:CGSizeMake(0.0, kBannerViewYShadowOffset)];
  [self.view.layer setShadowRadius:kBannerViewShadowRadius];
  [self.view.layer setShadowOpacity:kBannerViewShadowOpacity];
  // If dark mode is set when the banner is presented, the semantic color will
  // need to be set here.
  if (@available(iOS 13, *)) {
    [self.traitCollection performAsCurrentTraitCollection:^{
      [self.view.layer
          setShadowColor:[UIColor colorNamed:kToolbarShadowColor].CGColor];
    }];
  } else {
    [self.view.layer setShadowColor:[UIColor blackColor].CGColor];
  }
  self.view.accessibilityIdentifier = kInfobarBannerViewIdentifier;
  self.view.isAccessibilityElement = YES;
  self.view.accessibilityLabel = [self accessibilityLabel];
  self.view.accessibilityCustomActions = [self accessibilityActions];

  // Icon setup.
  UIView* iconContainerView = nil;
  if (self.iconImage) {
    self.iconImage = [self.iconImage
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    UIImageView* iconImageView =
        [[UIImageView alloc] initWithImage:self.iconImage];
    iconImageView.contentMode = UIViewContentModeScaleAspectFit;
    iconImageView.translatesAutoresizingMaskIntoConstraints = NO;

    UIView* backgroundIconView =
        [[UIView alloc] initWithFrame:iconImageView.frame];
    backgroundIconView.backgroundColor = [UIColor colorNamed:kBlueHaloColor];
    backgroundIconView.layer.cornerRadius = kIconCornerRadius;
    backgroundIconView.translatesAutoresizingMaskIntoConstraints = NO;

    iconContainerView = [[UIView alloc] init];
    [iconContainerView addSubview:backgroundIconView];
    [iconContainerView addSubview:iconImageView];
    iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;

    [NSLayoutConstraint activateConstraints:@[
      [backgroundIconView.centerXAnchor
          constraintEqualToAnchor:iconContainerView.centerXAnchor],
      [backgroundIconView.centerYAnchor
          constraintEqualToAnchor:iconContainerView.centerYAnchor],
      [backgroundIconView.widthAnchor constraintEqualToConstant:kIconWidth],
      [backgroundIconView.heightAnchor constraintEqualToConstant:kIconHeight],
      [iconImageView.centerXAnchor
          constraintEqualToAnchor:iconContainerView.centerXAnchor],
      [iconImageView.centerYAnchor
          constraintEqualToAnchor:iconContainerView.centerYAnchor],
      [iconImageView.widthAnchor constraintEqualToConstant:kIconWidth],
      [iconContainerView.widthAnchor
          constraintEqualToAnchor:backgroundIconView.widthAnchor],
    ]];
  }

  // Labels setup.
  self.titleLabel = [[UILabel alloc] init];
  self.titleLabel.text = self.titleText;
  self.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.titleLabel.adjustsFontForContentSizeCategory = YES;
  self.titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  self.titleLabel.numberOfLines = 0;
  self.titleLabel.baselineAdjustment = UIBaselineAdjustmentAlignCenters;
  [self.titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];

  self.subTitleLabel = [[UILabel alloc] init];
  self.subTitleLabel.text = self.subTitleText;
  self.subTitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  self.subTitleLabel.adjustsFontForContentSizeCategory = YES;
  self.subTitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  self.subTitleLabel.numberOfLines = 0;
  // If |self.subTitleText| hasn't been set or is empty, hide the label to keep
  // the title label centered in the Y axis.
  self.subTitleLabel.hidden = ![self.subTitleText length];

  UIStackView* labelsStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ self.titleLabel, self.subTitleLabel ]];
  labelsStackView.axis = UILayoutConstraintAxisVertical;

  // Button setup.
  self.infobarButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [self.infobarButton setTitle:self.buttonText forState:UIControlStateNormal];
  self.infobarButton.titleLabel.font = [[UIFontMetrics defaultMetrics]
      scaledFontForFont:[UIFont
                            preferredFontForTextStyle:UIFontTextStyleHeadline]
       maximumPointSize:kButtonMaxFontSize];
  self.infobarButton.titleLabel.numberOfLines = 0;
  self.infobarButton.titleLabel.textAlignment = NSTextAlignmentCenter;
  [self.infobarButton addTarget:self
                         action:@selector(bannerInfobarButtonWasPressed:)
               forControlEvents:UIControlEventTouchUpInside];
  self.infobarButton.accessibilityIdentifier =
      kInfobarBannerAcceptButtonIdentifier;

  UIView* buttonSeparator = [[UIView alloc] init];
  buttonSeparator.translatesAutoresizingMaskIntoConstraints = NO;
  buttonSeparator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  [self.infobarButton addSubview:buttonSeparator];

  // Container Stack setup.
  UIStackView* containerStack = [[UIStackView alloc] init];
  // Check if it should have an icon.
  if (iconContainerView) {
    [containerStack addArrangedSubview:iconContainerView];
  }
  // Add labels.
  [containerStack addArrangedSubview:labelsStackView];
    // Open Modal Button setup.
  self.openModalButton = [UIButton buttonWithType:UIButtonTypeSystem];
  UIImage* gearImage = [[UIImage imageNamed:@"infobar_settings_icon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [self.openModalButton setImage:gearImage forState:UIControlStateNormal];
  self.openModalButton.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  [self.openModalButton addTarget:self
                           action:@selector(animateBannerTappedAndPresentModal)
                 forControlEvents:UIControlEventTouchUpInside];
  [self.openModalButton
      setContentHuggingPriority:UILayoutPriorityDefaultHigh
                        forAxis:UILayoutConstraintAxisHorizontal];
  [self.openModalButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  self.openModalButton.accessibilityIdentifier =
      kInfobarBannerOpenModalButtonIdentifier;
  [containerStack addArrangedSubview:self.openModalButton];
  // Hide open modal button if user shouldn't be allowed to open the modal.
  self.openModalButton.hidden = !self.presentsModal;

  // Add accept button.
  [containerStack addArrangedSubview:self.infobarButton];
  // Configure it.
  containerStack.axis = UILayoutConstraintAxisHorizontal;
  containerStack.spacing = kContainerStackSpacing;
  containerStack.distribution = UIStackViewDistributionFill;
  containerStack.alignment = UIStackViewAlignmentFill;
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  containerStack.layoutMarginsRelativeArrangement = YES;
  containerStack.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      kContainerStackVerticalPadding, 0, kContainerStackVerticalPadding, 0);
  containerStack.insetsLayoutMarginsFromSafeArea = NO;
  [self.view addSubview:containerStack];

  // Constraints setup.
  [NSLayoutConstraint activateConstraints:@[
    // Container Stack.
    [containerStack.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kContainerStackHorizontalPadding],
    [containerStack.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [containerStack.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [containerStack.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    // Button.
    [self.infobarButton.widthAnchor constraintEqualToConstant:kButtonWidth],
    [buttonSeparator.widthAnchor
        constraintEqualToConstant:kButtonSeparatorWidth],
    [buttonSeparator.leadingAnchor
        constraintEqualToAnchor:self.infobarButton.leadingAnchor],
    [buttonSeparator.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [buttonSeparator.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
  ]];

  // Gestures setup.
  UIPanGestureRecognizer* panGestureRecognizer =
      [[UIPanGestureRecognizer alloc] init];
  [panGestureRecognizer addTarget:self action:@selector(handleGestures:)];
  [panGestureRecognizer setMaximumNumberOfTouches:1];
  [self.view addGestureRecognizer:panGestureRecognizer];

  UILongPressGestureRecognizer* longPressGestureRecognizer =
      [[UILongPressGestureRecognizer alloc] init];
  [longPressGestureRecognizer addTarget:self action:@selector(handleGestures:)];
  longPressGestureRecognizer.minimumPressDuration =
      kLongPressTimeDurationInSeconds;
  [self.view addGestureRecognizer:longPressGestureRecognizer];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.metricsRecorder recordBannerEvent:MobileMessagesBannerEvent::Presented];
  self.bannerAppearedTime = [NSDate timeIntervalSinceReferenceDate];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  // Call recordBannerOnScreenTime on viewWillDisappear since viewDidDisappear
  // is called after the dismissal animation has occured.
  [self recordBannerOnScreenTime];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.metricsRecorder recordBannerEvent:MobileMessagesBannerEvent::Dismissed];
  [self.delegate infobarBannerWasDismissed];
  [super viewDidDisappear:animated];
}

// This is triggered when dark mode changes while the banner is already
// presented.
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 13, *)) {
    if ([self.traitCollection
            hasDifferentColorAppearanceComparedToTraitCollection:
                previousTraitCollection]) {
      [self.view.layer
          setShadowColor:[UIColor colorNamed:kToolbarShadowColor].CGColor];
    }
  }
}

#pragma mark - Public Methods

- (void)dismissWhenInteractionIsFinished {
  if (!self.touchInProgress) {
    [self.metricsRecorder
        recordBannerDismissType:MobileMessagesBannerDismissType::TimedOut];
    [self.delegate dismissInfobarBanner:self
                               animated:YES
                             completion:nil
                          userInitiated:NO];
  }
  self.shouldDismissAfterTouchesEnded = YES;
}

#pragma mark - Getters/Setters

- (void)setTitleText:(NSString*)titleText {
  _titleText = titleText;
  if (self.titleLabel) {
    self.titleLabel.text = _titleText;
  }
}

- (void)setSubTitleText:(NSString*)subTitleText {
  _subTitleText = subTitleText;
  if (self.subTitleLabel) {
    self.subTitleLabel.text = _subTitleText;
  }
}

- (void)setButtonText:(NSString*)buttonText {
  _buttonText = buttonText;
  if (self.infobarButton) {
    [self.infobarButton setTitle:_buttonText forState:UIControlStateNormal];
  }
}

- (void)setPresentsModal:(BOOL)presentsModal {
  // TODO(crbug.com/961343): Write a test for setting this to NO;
  if (_presentsModal == presentsModal) {
    return;
  }
  _presentsModal = presentsModal;
  self.openModalButton.hidden = !presentsModal;
  self.view.accessibilityCustomActions = [self accessibilityActions];
}

#pragma mark - Private Methods

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  [self.interactionDelegate infobarBannerStartedInteraction];
  [self.metricsRecorder recordBannerEvent:MobileMessagesBannerEvent::Accepted];
  [self.delegate bannerInfobarButtonWasPressed:sender];
}

- (void)handleGestures:(UILongPressGestureRecognizer*)gesture {
  CGPoint touchLocation = [gesture locationInView:self.view];

  if (gesture.state == UIGestureRecognizerStateBegan) {
    [self.interactionDelegate infobarBannerStartedInteraction];
    [self.metricsRecorder recordBannerEvent:MobileMessagesBannerEvent::Handled];
    self.originalCenter = self.view.center;
    self.touchInProgress = YES;
    self.startingTouch = touchLocation;
    [self animateBannerToScaleUpState];
  } else if (gesture.state == UIGestureRecognizerStateChanged) {
    // Don't allow the banner to be dragged down past its original position.
    CGFloat newYPosition =
        self.view.center.y + touchLocation.y - self.startingTouch.y;
    if (newYPosition < self.originalCenter.y) {
      self.view.center = CGPointMake(self.view.center.x, newYPosition);
    }
  }

  if (gesture.state == UIGestureRecognizerStateEnded) {
    [self animateBannerToOriginalStateWithDuration:
              kSelectBannerAnimationDurationInSeconds
                                        completion:nil];
    // If dragged up by more than kChangeInPositionForDismissal at the time
    // the gesture ended, OR |self.shouldDismissAfterTouchesEnded| is YES.
    // Dismiss the banner.
    BOOL dragUpExceededThreshold = (self.view.center.y - self.originalCenter.y -
                                        kChangeInPositionForDismissal <
                                    0);
    if (dragUpExceededThreshold || self.shouldDismissAfterTouchesEnded) {
      if (dragUpExceededThreshold) {
        [self.metricsRecorder
            recordBannerDismissType:MobileMessagesBannerDismissType::SwipedUp];
        [self.delegate dismissInfobarBanner:self
                                   animated:YES
                                 completion:nil
                              userInitiated:YES];
      } else {
        [self.metricsRecorder
            recordBannerDismissType:MobileMessagesBannerDismissType::TimedOut];
        [self.delegate dismissInfobarBanner:self
                                   animated:YES
                                 completion:nil
                              userInitiated:NO];
      }
    } else {
      [self.metricsRecorder
          recordBannerEvent:MobileMessagesBannerEvent::ReturnedToOrigin];
      [self animateBannerToOriginalPosition];
    }
  }

  if (gesture.state == UIGestureRecognizerStateCancelled) {
    // Reset the superview transform so its frame is valid again.
    self.view.superview.transform = CGAffineTransformIdentity;
  }
}

// Animate the Banner being selected by scaling it up.
- (void)animateBannerToScaleUpState {
  [UIView animateWithDuration:kSelectBannerAnimationDurationInSeconds
                   animations:^{
                     self.view.superview.transform = CGAffineTransformMakeScale(
                         kSelectedBannerViewScale, kSelectedBannerViewScale);
                     [self.view.layer
                         setShadowOffset:CGSizeMake(
                                             0.0,
                                             kSelectedBannerViewYShadowOffset)];
                   }
                   completion:nil];
}

// Animate the Banner back to its original size and styling.
- (void)animateBannerToOriginalStateWithDuration:(NSTimeInterval)duration
                                      completion:(ProceduralBlock)completion {
  [UIView animateWithDuration:duration
      animations:^{
        self.view.superview.transform = CGAffineTransformIdentity;
        [self.view.layer
            setShadowOffset:CGSizeMake(0.0, kBannerViewYShadowOffset)];
      }
      completion:^(BOOL finished) {
        if (completion)
          completion();
      }];
}

// Animate the banner back to its original position.
- (void)animateBannerToOriginalPosition {
  [UIView animateWithDuration:kSelectBannerAnimationDurationInSeconds
                   animations:^{
                     self.view.center = self.originalCenter;
                   }
                   completion:nil];
}

// Animate the Banner being tapped by scaling it down and then to its original
// state. After the animation, it presents the Infobar Modal.
- (void)animateBannerTappedAndPresentModal {
  DCHECK(self.presentsModal);
  [self.interactionDelegate infobarBannerStartedInteraction];
  // TODO(crbug.com/961343): Interrupt this animation in case the Banner needs
  // to be dismissed mid tap (Currently it will be dismmissed after the
  // animation).
  [UIView animateWithDuration:kTappedBannerAnimationDurationInSeconds
      animations:^{
        self.view.superview.transform = CGAffineTransformMakeScale(
            kTappedBannerViewScale, kTappedBannerViewScale);
        [self.view.layer
            setShadowOffset:CGSizeMake(0.0, kSelectedBannerViewYShadowOffset)];
      }
      completion:^(BOOL finished) {
        [self
            animateBannerToOriginalStateWithDuration:
                kTappedBannerAnimationDurationInSeconds
                                          completion:^{
                                            [self presentInfobarModalAfterTap];
                                          }];
      }];
}

- (void)presentInfobarModalAfterTap {
  DCHECK(self.presentsModal);
  base::RecordAction(base::UserMetricsAction("MobileMessagesBannerTapped"));
  [self.metricsRecorder
      recordBannerDismissType:MobileMessagesBannerDismissType::TappedToModal];
  [self recordBannerOnScreenTime];
  [self.delegate presentInfobarModalFromBanner];
}

// Records the banner on screen time. This method should be called as soon as
// its know that the banner will not be visible. This might happen before
// viewWillDissapear since presenting a Modal makes the banner invisible but
// doesn't call viewWillDissapear.
- (void)recordBannerOnScreenTime {
  if (!self.bannerOnScreenTimeWasRecorded) {
    double duration =
        [NSDate timeIntervalSinceReferenceDate] - self.bannerAppearedTime;
    [self.metricsRecorder recordBannerOnScreenDuration:duration];
    self.bannerOnScreenTimeWasRecorded = YES;
  }
}

#pragma mark - Accessibility

- (NSArray*)accessibilityActions {
  UIAccessibilityCustomAction* acceptAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:self.buttonText
                target:self
              selector:@selector(acceptInfobar)];

  UIAccessibilityCustomAction* dismissAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_INFOBAR_BANNER_DISMISS_HINT)
                target:self
              selector:@selector(dismiss)];

  NSMutableArray* accessibilityActions =
      [@[ acceptAction, dismissAction ] mutableCopy];

  if (self.presentsModal) {
    UIAccessibilityCustomAction* modalAction =
        [[UIAccessibilityCustomAction alloc]
            initWithName:l10n_util::GetNSString(
                             IDS_IOS_INFOBAR_BANNER_OPTIONS_HINT)
                  target:self
                selector:@selector(triggerInfobarModal)];
    [accessibilityActions addObject:modalAction];
  }

  return accessibilityActions;
}

// A11y Custom actions selectors need to return a BOOL.
- (BOOL)acceptInfobar {
  [self bannerInfobarButtonWasPressed:nil];
  return NO;
}

- (BOOL)triggerInfobarModal {
  [self presentInfobarModalAfterTap];
  return NO;
}

- (BOOL)dismiss {
  [self.delegate dismissInfobarBanner:self
                             animated:YES
                           completion:nil
                        userInitiated:YES];
  return NO;
}

- (NSString*)accessibilityLabel {
  if ([self.optionalAccessibilityLabel length])
    return self.optionalAccessibilityLabel;
  NSString* accessibilityLabel = self.titleText;
  if ([self.subTitleText length]) {
    accessibilityLabel =
        [NSString stringWithFormat:@"%@,%@", self.titleText, self.subTitleText];
  }
  return accessibilityLabel;
}

@end
