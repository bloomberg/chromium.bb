// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory_view.h"

#import <QuartzCore/QuartzCore.h>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/form_input_navigator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"
#import "ios/chrome/browser/ui/image_util/image_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The alpha value of the background color.
const CGFloat kBackgroundColorAlpha = 1.0;

// The width of the separators of the previous and next buttons.
const CGFloat kNavigationButtonSeparatorWidth = 1;

// The width of the shadow part of the navigation area separator.
const CGFloat kNavigationAreaSeparatorShadowWidth = 2;

// The width of the navigation area / custom view separator asset.
const CGFloat kNavigationAreaSeparatorWidth = 1;

// The width for the white gradient UIImageView.
constexpr CGFloat ManualFillGradientWidth = 44;

// The margin for the white gradient UIImageView.
constexpr CGFloat ManualFillGradientMargin = 14;

// The spacing between the items in the navigation view.
constexpr CGFloat ManualFillNavigationItemSpacing = 4;

// The left content inset for the close button.
constexpr CGFloat ManualFillCloseButtonLeftInset = 7;

// The right content inset for the close button.
constexpr CGFloat ManualFillCloseButtonRightInset = 15;

// The height for the top and bottom sepparator lines.
constexpr CGFloat ManualFillSeparatorHeight = 0.5;

}  // namespace

@interface FormInputAccessoryView ()

// The navigation delegate if any.
@property(nonatomic, nullable, weak) id<FormInputAccessoryViewDelegate>
    delegate;

@property(nonatomic, weak) UIButton* previousButton;

@property(nonatomic, weak) UIButton* nextButton;

@property(nonatomic, weak) UIView* leadingView;

@end

@implementation FormInputAccessoryView

#pragma mark - Public

- (void)setUpWithLeadingView:(UIView*)leadingView
          customTrailingView:(UIView*)customTrailingView {
  [self setUpWithLeadingView:leadingView
          customTrailingView:customTrailingView
          navigationDelegate:nil];
}

- (void)setUpWithLeadingView:(UIView*)leadingView
          navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate {
  [self setUpWithLeadingView:leadingView
          customTrailingView:nil
          navigationDelegate:delegate];
}

#pragma mark - UIInputViewAudioFeedback

- (BOOL)enableInputClicksWhenVisible {
  return YES;
}

#pragma mark - Private Methods

- (void)closeButtonTapped {
  [self.delegate formInputAccessoryViewDidTapCloseButton:self];
}

- (void)nextButtonTapped {
  [self.delegate formInputAccessoryViewDidTapNextButton:self];
}

- (void)previousButtonTapped {
  [self.delegate formInputAccessoryViewDidTapPreviousButton:self];
}

// Sets up the view with the given |leadingView|. If |delegate| is not nil,
// navigation controls are shown on the right and use |delegate| for actions.
// Else navigation controls are replaced with |customTrailingView|. If none of
// |delegate| and |customTrailingView| is set, leadingView will take all the
// space.
- (void)setUpWithLeadingView:(UIView*)leadingView
          customTrailingView:(UIView*)customTrailingView
          navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate {
  DCHECK(!self.subviews.count);  // This should only be called once.
  DCHECK(leadingView);
  self.leadingView = leadingView;

  if (!autofill::features::IsPasswordManualFallbackEnabled()) {
    [[self class] addBackgroundImageInView:self
                             withImageName:@"autofill_keyboard_background"];
  }
  leadingView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* trailingView;
  if (delegate) {
    self.delegate = delegate;
    trailingView = [self viewForNavigationButtons];
  } else {
    trailingView = customTrailingView;
  }

  // If there is no trailing view, set the leading view as the only view and
  // return early.
  if (!trailingView) {
    [self addSubview:leadingView];
    AddSameConstraints(self, leadingView);
    return;
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;
  UIView* leadingViewContainer = [[UIView alloc] init];
  leadingViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:leadingViewContainer];
  [leadingViewContainer addSubview:leadingView];
  AddSameConstraints(leadingViewContainer, leadingView);

  trailingView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:trailingView];

  id<LayoutGuideProvider> layoutGuide = self.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [leadingViewContainer.topAnchor constraintEqualToAnchor:self.topAnchor],
    [leadingViewContainer.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor],
    [leadingViewContainer.leadingAnchor
        constraintEqualToAnchor:layoutGuide.leadingAnchor],
    [trailingView.trailingAnchor
        constraintEqualToAnchor:layoutGuide.trailingAnchor],
    [trailingView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [trailingView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
  ]];

  if (autofill::features::IsPasswordManualFallbackEnabled()) {
    self.backgroundColor = UIColor.whiteColor;

    UIImage* gradientImage = [[UIImage imageNamed:@"mf_gradient"]
        resizableImageWithCapInsets:UIEdgeInsetsZero
                       resizingMode:UIImageResizingModeStretch];
    UIImageView* gradientView =
        [[UIImageView alloc] initWithImage:gradientImage];
    gradientView.translatesAutoresizingMaskIntoConstraints = NO;
    if (base::i18n::IsRTL()) {
      gradientView.transform = CGAffineTransformMakeRotation(M_PI);
    }
    [self insertSubview:gradientView belowSubview:trailingView];

    UIView* topGrayLine = [[UIView alloc] init];
    topGrayLine.backgroundColor = UIColor.cr_manualFillSeparatorColor;
    topGrayLine.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:topGrayLine];

    UIView* bottomGrayLine = [[UIView alloc] init];
    bottomGrayLine.backgroundColor = UIColor.cr_manualFillSeparatorColor;
    bottomGrayLine.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:bottomGrayLine];

    [NSLayoutConstraint activateConstraints:@[
      [topGrayLine.topAnchor constraintEqualToAnchor:self.topAnchor],
      [topGrayLine.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [topGrayLine.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [topGrayLine.heightAnchor
          constraintEqualToConstant:ManualFillSeparatorHeight],

      [bottomGrayLine.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [bottomGrayLine.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [bottomGrayLine.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [bottomGrayLine.heightAnchor
          constraintEqualToConstant:ManualFillSeparatorHeight],

      [gradientView.topAnchor constraintEqualToAnchor:trailingView.topAnchor],
      [gradientView.bottomAnchor
          constraintEqualToAnchor:trailingView.bottomAnchor],
      [gradientView.widthAnchor
          constraintEqualToConstant:ManualFillGradientWidth],
      [gradientView.trailingAnchor
          constraintEqualToAnchor:trailingView.leadingAnchor
                         constant:ManualFillGradientMargin],

      [leadingViewContainer.trailingAnchor
          constraintEqualToAnchor:trailingView.leadingAnchor],
    ]];
  } else {
    [leadingViewContainer.trailingAnchor
        constraintEqualToAnchor:trailingView.leadingAnchor
                       constant:kNavigationAreaSeparatorShadowWidth]
        .active = YES;
  }
}

UIImage* ButtonImage(NSString* name) {
  UIImage* rawImage = [UIImage imageNamed:name];
  return StretchableImageFromUIImage(rawImage, 1, 0);
}

// Returns a view that shows navigation buttons.
- (UIView*)viewForNavigationButtons {
  if (autofill::features::IsPasswordManualFallbackEnabled()) {
    UIButton* previousButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [previousButton setImage:[UIImage imageNamed:@"mf_arrow_up"]
                    forState:UIControlStateNormal];
    previousButton.tintColor = UIColor.cr_manualFillTintColor;
    [previousButton addTarget:self
                       action:@selector(previousButtonTapped)
             forControlEvents:UIControlEventTouchUpInside];
    previousButton.enabled = NO;
    NSString* previousButtonAccessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_PREVIOUS_FIELD);
    [previousButton setAccessibilityLabel:previousButtonAccessibilityLabel];

    UIButton* nextButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [nextButton setImage:[UIImage imageNamed:@"mf_arrow_down"]
                forState:UIControlStateNormal];
    nextButton.tintColor = UIColor.cr_manualFillTintColor;
    [nextButton addTarget:self
                   action:@selector(nextButtonTapped)
         forControlEvents:UIControlEventTouchUpInside];
    nextButton.enabled = NO;
    NSString* nextButtonAccessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_NEXT_FIELD);
    [nextButton setAccessibilityLabel:nextButtonAccessibilityLabel];

    UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
    NSString* title = l10n_util::GetNSString(IDS_IOS_AUTOFILL_INPUT_BAR_DONE);
    [closeButton setTitle:title forState:UIControlStateNormal];
    closeButton.tintColor = UIColor.cr_manualFillTintColor;
    [closeButton addTarget:self
                    action:@selector(closeButtonTapped)
          forControlEvents:UIControlEventTouchUpInside];
    closeButton.contentEdgeInsets = UIEdgeInsetsMake(
        0, ManualFillCloseButtonLeftInset, 0, ManualFillCloseButtonRightInset);
    NSString* closeButtonAccessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_HIDE_KEYBOARD);
    [closeButton setAccessibilityLabel:closeButtonAccessibilityLabel];

    self.nextButton = nextButton;
    self.previousButton = previousButton;

    UIStackView* navigationView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ previousButton, nextButton, closeButton ]];
    navigationView.spacing = ManualFillNavigationItemSpacing;
    return navigationView;
  }

  UIView* navView = [[UIView alloc] init];
  navView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* separator =
      [[self class] createImageViewWithImageName:@"autofill_left_sep"
                                          inView:navView];

  UIButton* previousButton = [self
      addKeyboardNavButtonWithNormalImage:ButtonImage(@"autofill_prev")
                             pressedImage:ButtonImage(@"autofill_prev_pressed")
                            disabledImage:ButtonImage(@"autofill_prev_inactive")
                                   target:self
                                   action:@selector(previousButtonTapped)
                                  enabled:NO
                                   inView:navView];
  [previousButton
      setAccessibilityLabel:l10n_util::GetNSString(
                                IDS_IOS_AUTOFILL_ACCNAME_PREVIOUS_FIELD)];

  // Add internal separator.
  UIView* internalSeparator =
      [[self class] createImageViewWithImageName:@"autofill_middle_sep"
                                          inView:navView];

  UIButton* nextButton = [self
      addKeyboardNavButtonWithNormalImage:ButtonImage(@"autofill_next")
                             pressedImage:ButtonImage(@"autofill_next_pressed")
                            disabledImage:ButtonImage(@"autofill_next_inactive")
                                   target:self
                                   action:@selector(nextButtonTapped)
                                  enabled:NO
                                   inView:navView];
  [nextButton setAccessibilityLabel:l10n_util::GetNSString(
                                        IDS_IOS_AUTOFILL_ACCNAME_NEXT_FIELD)];

  // Add internal separator.
  UIView* internalSeparator2 =
      [[self class] createImageViewWithImageName:@"autofill_middle_sep"
                                          inView:navView];

  UIButton* closeButton = [self
      addKeyboardNavButtonWithNormalImage:ButtonImage(@"autofill_close")
                             pressedImage:ButtonImage(@"autofill_close_pressed")
                            disabledImage:nil
                                   target:self
                                   action:@selector(closeButtonTapped)
                                  enabled:YES
                                   inView:navView];
  [closeButton
      setAccessibilityLabel:l10n_util::GetNSString(
                                IDS_IOS_AUTOFILL_ACCNAME_HIDE_KEYBOARD)];

  ApplyVisualConstraintsWithMetrics(
      @[
        (@"H:|[separator1(==areaSeparatorWidth)][previousButton][separator2(=="
         @"buttonSeparatorWidth)][nextButton][internalSeparator2("
         @"buttonSeparatorWidth)][closeButton]|"),
        @"V:|-(topPadding)-[separator1]|",
        @"V:|-(topPadding)-[previousButton]|",
        @"V:|-(topPadding)-[previousButton]|",
        @"V:|-(topPadding)-[separator2]|", @"V:|-(topPadding)-[nextButton]|",
        @"V:|-(topPadding)-[internalSeparator2]|",
        @"V:|-(topPadding)-[closeButton]|"
      ],
      @{
        @"separator1" : separator,
        @"previousButton" : previousButton,
        @"separator2" : internalSeparator,
        @"nextButton" : nextButton,
        @"internalSeparator2" : internalSeparator2,
        @"closeButton" : closeButton
      },
      @{

        @"areaSeparatorWidth" : @(kNavigationAreaSeparatorWidth),
        @"buttonSeparatorWidth" : @(kNavigationButtonSeparatorWidth),
        @"topPadding" : @(1)
      });

  self.nextButton = nextButton;
  self.previousButton = previousButton;
  return navView;
}

// Adds a button in |view| that has |normalImage| for  state
// UIControlStateNormal, a |pressedImage| for states UIControlStateSelected and
// UIControlStateHighlighted, and an optional |disabledImage| for
// UIControlStateDisabled.
- (UIButton*)addKeyboardNavButtonWithNormalImage:(UIImage*)normalImage
                                    pressedImage:(UIImage*)pressedImage
                                   disabledImage:(UIImage*)disabledImage
                                          target:(id)target
                                          action:(SEL)action
                                         enabled:(BOOL)enabled
                                          inView:(UIView*)view {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  [button setBackgroundImage:normalImage forState:UIControlStateNormal];
  [button setBackgroundImage:pressedImage forState:UIControlStateSelected];
  [button setBackgroundImage:pressedImage forState:UIControlStateHighlighted];
  if (disabledImage)
    [button setBackgroundImage:disabledImage forState:UIControlStateDisabled];

  CALayer* layer = [button layer];
  layer.borderWidth = 0;
  layer.borderColor = [[UIColor blackColor] CGColor];
  button.enabled = enabled;
  [button addTarget:target
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  [button.heightAnchor constraintEqualToAnchor:button.widthAnchor].active = YES;
  [view addSubview:button];
  return button;
}

// Adds a background image to |view|. The supplied image is stretched to fit the
// space by stretching the content its horizontal and vertical centers.
+ (void)addBackgroundImageInView:(UIView*)view
                   withImageName:(NSString*)imageName {
  UIImage* backgroundImage = StretchableImageNamed(imageName);

  UIImageView* backgroundImageView = [[UIImageView alloc] init];
  backgroundImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [backgroundImageView setImage:backgroundImage];
  [backgroundImageView setAlpha:kBackgroundColorAlpha];
  [view addSubview:backgroundImageView];
  [view sendSubviewToBack:backgroundImageView];
  AddSameConstraints(view, backgroundImageView);
}

// Adds an image view in |view| with an image named |imageName|.
+ (UIView*)createImageViewWithImageName:(NSString*)imageName
                                 inView:(UIView*)view {
  UIImage* image =
      StretchableImageFromUIImage([UIImage imageNamed:imageName], 0, 0);
  UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [view addSubview:imageView];
  return imageView;
}

@end
