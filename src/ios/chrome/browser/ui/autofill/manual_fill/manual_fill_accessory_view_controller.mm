// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"

#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

static NSTimeInterval MFAnimationDuration = 0.20;

@interface ManualFillAccessoryViewController ()

@property(nonatomic, readonly, weak)
    id<ManualFillAccessoryViewControllerDelegate>
        delegate;

@property(nonatomic, strong) UIButton* keyboardButton;
@property(nonatomic, strong) UIButton* passwordButton;
@property(nonatomic, strong) UIButton* cardsButton;
@property(nonatomic, strong) UIButton* accountButton;

@end

@implementation ManualFillAccessoryViewController

@synthesize delegate = _delegate;
@synthesize keyboardButton = _keyboardButton;
@synthesize passwordButton = _passwordButton;
@synthesize cardsButton = _cardsButton;
@synthesize accountButton = _accountButton;

- (instancetype)initWithDelegate:
    (id<ManualFillAccessoryViewControllerDelegate>)delegate {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

- (void)loadView {
  self.view = [[UIView alloc] init];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;

  UIColor* tintColor = [self activeTintColor];

  self.keyboardButton = [UIButton buttonWithType:UIButtonTypeSystem];
  UIImage* keyboardImage = [UIImage imageNamed:@"ic_keyboard"];
  [self.keyboardButton setImage:keyboardImage forState:UIControlStateNormal];
  self.keyboardButton.tintColor = tintColor;
  self.keyboardButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.keyboardButton addTarget:self
                          action:@selector(keyboardButtonPressed)
                forControlEvents:UIControlEventTouchUpInside];

  self.passwordButton = [UIButton buttonWithType:UIButtonTypeSystem];
  UIImage* keyImage = [UIImage imageNamed:@"ic_vpn_key"];
  [self.passwordButton setImage:keyImage forState:UIControlStateNormal];
  self.passwordButton.tintColor = tintColor;
  self.passwordButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.passwordButton addTarget:self
                          action:@selector(passwordButtonPressed)
                forControlEvents:UIControlEventTouchUpInside];

  self.cardsButton = [UIButton buttonWithType:UIButtonTypeSystem];
  UIImage* cardImage = [UIImage imageNamed:@"ic_credit_card"];
  [self.cardsButton setImage:cardImage forState:UIControlStateNormal];
  self.cardsButton.tintColor = tintColor;
  self.cardsButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.cardsButton addTarget:self
                       action:@selector(cardButtonPressed)
             forControlEvents:UIControlEventTouchUpInside];

  self.accountButton = [UIButton buttonWithType:UIButtonTypeSystem];
  UIImage* accountImage = [UIImage imageNamed:@"addresses"];
  [self.accountButton setImage:accountImage forState:UIControlStateNormal];
  self.accountButton.tintColor = tintColor;
  self.accountButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.accountButton addTarget:self
                         action:@selector(accountButtonPressed)
               forControlEvents:UIControlEventTouchUpInside];

  NSLayoutXAxisAnchor* menuLeadingAnchor = self.view.leadingAnchor;
  if (@available(iOS 11, *)) {
    menuLeadingAnchor = self.view.safeAreaLayoutGuide.leadingAnchor;
  }
  NSLayoutXAxisAnchor* menuTrailingAnchor = self.view.trailingAnchor;
  if (@available(iOS 11, *)) {
    menuTrailingAnchor = self.view.safeAreaLayoutGuide.trailingAnchor;
  }

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    self.keyboardButton, self.passwordButton, self.accountButton,
    self.cardsButton
  ]];
  stackView.spacing = 10;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:stackView];

  [NSLayoutConstraint activateConstraints:@[
    // Vertical constraints.
    [stackView.heightAnchor constraintEqualToAnchor:self.view.heightAnchor],
    [stackView.topAnchor constraintEqualToAnchor:self.view.topAnchor],

    // Horizontal constraints.
    [stackView.leadingAnchor constraintEqualToAnchor:menuLeadingAnchor
                                            constant:10],
    [stackView.trailingAnchor constraintEqualToAnchor:menuTrailingAnchor],
  ]];
  self.keyboardButton.hidden = YES;
  self.keyboardButton.alpha = 0.0;
}

- (void)reset {
  [self resetTintColors];
  self.keyboardButton.hidden = YES;
  self.keyboardButton.alpha = 0.0;
}

// Resets the colors of all the icons to the active color.
- (void)resetTintColors {
  UIColor* activeTintColor = [self activeTintColor];
  [self.accountButton setTintColor:activeTintColor];
  [self.passwordButton setTintColor:activeTintColor];
  [self.cardsButton setTintColor:activeTintColor];
}

- (UIColor*)activeTintColor {
  return [UIColor.blackColor colorWithAlphaComponent:0.5];
}

- (void)animateKeyboardButtonHidden:(BOOL)hidden {
  [UIView animateWithDuration:MFAnimationDuration
                   animations:^{
                     if (hidden) {
                       self.keyboardButton.hidden = YES;
                       self.keyboardButton.alpha = 0.0;
                     } else {
                       self.keyboardButton.hidden = NO;
                       self.keyboardButton.alpha = 1.0;
                     }
                   }];
}

- (void)keyboardButtonPressed {
  [self animateKeyboardButtonHidden:YES];
  [self resetTintColors];
  [self.delegate keyboardButtonPressed];
}

- (void)passwordButtonPressed {
  [self animateKeyboardButtonHidden:NO];
  [self resetTintColors];
  [self.passwordButton setTintColor:UIColor.cr_manualFillTintColor];
  [self.delegate passwordButtonPressed];
}

- (void)cardButtonPressed {
  [self animateKeyboardButtonHidden:NO];
  [self resetTintColors];
  [self.cardsButton setTintColor:UIColor.cr_manualFillTintColor];
  [self.delegate cardButtonPressed];
}

- (void)accountButtonPressed {
  [self animateKeyboardButtonHidden:NO];
  [self resetTintColors];
  [self.accountButton setTintColor:UIColor.cr_manualFillTintColor];
  [self.delegate accountButtonPressed];
}

@end
