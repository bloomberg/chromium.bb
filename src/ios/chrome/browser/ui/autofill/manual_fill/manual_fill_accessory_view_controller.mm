// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"

#include "base/metrics/user_metrics.h"
#include "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace manual_fill {

NSString* const AccessoryKeyboardAccessibilityIdentifier =
    @"kManualFillAccessoryKeyboardAccessibilityIdentifier";
NSString* const AccessoryPasswordAccessibilityIdentifier =
    @"kManualFillAccessoryPasswordAccessibilityIdentifier";
NSString* const AccessoryAddressAccessibilityIdentifier =
    @"kManualFillAccessoryAddressAccessibilityIdentifier";
NSString* const AccessoryCreditCardAccessibilityIdentifier =
    @"kManualFillAccessoryCreditCardAccessibilityIdentifier";

}  // namespace manual_fill

namespace {

// The inset on the left before the icons start.
constexpr CGFloat ManualFillIconsLeftInset = 10;

// The inset on the right after the icons end.
constexpr CGFloat ManualFillIconsRightInset = 24;

}  // namespace

static NSTimeInterval MFAnimationDuration = 0.2;

@interface ManualFillAccessoryViewController ()

// Delegate to handle interactions.
@property(nonatomic, readonly, weak)
    id<ManualFillAccessoryViewControllerDelegate>
        delegate;

// The button to close manual fallback.
@property(nonatomic, strong) UIButton* keyboardButton;

// The button to open the passwords section.
@property(nonatomic, strong) UIButton* passwordButton;

// The button to open the credit cards section.
@property(nonatomic, strong) UIButton* cardsButton;

// The button to open the profiles section.
@property(nonatomic, strong) UIButton* accountButton;

@end

@implementation ManualFillAccessoryViewController

#pragma mark - Public

- (instancetype)initWithDelegate:
    (id<ManualFillAccessoryViewControllerDelegate>)delegate {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

- (void)resetAnimated:(BOOL)animated {
  [UIView animateWithDuration:animated ? MFAnimationDuration : 0
                   animations:^{
                     [self resetTintColors];
                     // Workaround the |hidden| property in stacked views.
                     if (!self.keyboardButton.hidden) {
                       self.keyboardButton.hidden = YES;
                       self.keyboardButton.alpha = 0.0;
                     }
                   }];
}

#pragma mark - Setters

- (void)setAddressButtonHidden:(BOOL)addressButtonHidden {
  if (addressButtonHidden == _addressButtonHidden) {
    return;
  }
  _accountButton.hidden = addressButtonHidden;
  _addressButtonHidden = addressButtonHidden;
}

- (void)setCreditCardButtonHidden:(BOOL)creditCardButtonHidden {
  if (creditCardButtonHidden == _creditCardButtonHidden) {
    return;
  }
  _cardsButton.hidden = creditCardButtonHidden;
  _creditCardButtonHidden = creditCardButtonHidden;
}

- (void)setPasswordButtonHidden:(BOOL)passwordButtonHidden {
  if (passwordButtonHidden == _passwordButtonHidden) {
    return;
  }
  _passwordButton.hidden = passwordButtonHidden;
  _passwordButtonHidden = passwordButtonHidden;
}

#pragma mark - Private

// Helper to create a system button with the passed data and |self| as the
// target. Such button has been configured to have some preset properties
- (UIButton*)manualFillButtonWithAction:(SEL)selector
                             ImageNamed:(NSString*)imageName
                accessibilityIdentifier:(NSString*)accessibilityIdentifier
                     accessibilityLabel:(NSString*)accessibilityLabel {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  UIImage* image = [UIImage imageNamed:imageName];
  [button setImage:image forState:UIControlStateNormal];
  button.tintColor = [self activeTintColor];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button addTarget:self
                action:selector
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier = accessibilityIdentifier;
  button.accessibilityLabel = accessibilityLabel;
  button.tintAdjustmentMode = UIViewTintAdjustmentModeNormal;
  return button;
}

- (void)loadView {
  self.view = [[UIView alloc] init];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;

  NSMutableArray<UIView*>* icons = [[NSMutableArray alloc] init];

  if (!IsIPadIdiom()) {
    self.keyboardButton = [self
        manualFillButtonWithAction:@selector(keyboardButtonPressed)
                        ImageNamed:@"mf_keyboard"
           accessibilityIdentifier:manual_fill::
                                       AccessoryKeyboardAccessibilityIdentifier
                accessibilityLabel:l10n_util::GetNSString(
                                       IDS_IOS_MANUAL_FALLBACK_SHOW_KEYBOARD)];
    [icons addObject:self.keyboardButton];
    self.keyboardButton.hidden = YES;
    self.keyboardButton.alpha = 0.0;
  }

  self.passwordButton = [self
      manualFillButtonWithAction:@selector(passwordButtonPressed:)
                      ImageNamed:@"ic_vpn_key"
         accessibilityIdentifier:manual_fill::
                                     AccessoryPasswordAccessibilityIdentifier
              accessibilityLabel:l10n_util::GetNSString(
                                     IDS_IOS_MANUAL_FALLBACK_SHOW_PASSWORDS)];

  self.passwordButton.hidden = self.isPasswordButtonHidden;
  self.passwordButton.contentEdgeInsets = UIEdgeInsetsMake(0, 2, 0, 2);
  [icons addObject:self.passwordButton];

  if (autofill::features::IsAutofillManualFallbackEnabled()) {
    self.cardsButton =
        [self manualFillButtonWithAction:@selector(cardButtonPressed:)
                              ImageNamed:@"ic_credit_card"
                 accessibilityIdentifier:
                     manual_fill::AccessoryCreditCardAccessibilityIdentifier
                      accessibilityLabel:
                          l10n_util::GetNSString(
                              IDS_IOS_MANUAL_FALLBACK_SHOW_CREDIT_CARDS)];
    self.cardsButton.hidden = self.isCreditCardButtonHidden;
    [icons addObject:self.cardsButton];

    self.accountButton = [self
        manualFillButtonWithAction:@selector(accountButtonPressed:)
                        ImageNamed:@"ic_place"
           accessibilityIdentifier:manual_fill::
                                       AccessoryAddressAccessibilityIdentifier
                accessibilityLabel:l10n_util::GetNSString(
                                       IDS_IOS_MANUAL_FALLBACK_SHOW_ADDRESSES)];

    self.accountButton.hidden = self.isAddressButtonHidden;
    [icons addObject:self.accountButton];
  }
  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:icons];
  stackView.spacing = 10;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:stackView];

  id<LayoutGuideProvider> safeAreaLayoutGuide = self.view.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    // Vertical constraints.
    [stackView.heightAnchor constraintEqualToAnchor:self.view.heightAnchor],
    [stackView.topAnchor constraintEqualToAnchor:self.view.topAnchor],

    // Horizontal constraints.
    [stackView.leadingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                       constant:ManualFillIconsLeftInset],
    [safeAreaLayoutGuide.trailingAnchor
        constraintEqualToAnchor:stackView.trailingAnchor
                       constant:ManualFillIconsRightInset],
  ]];
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
                     // Workaround setting more than once the |hidden| property
                     // in stacked views.
                     if (self.keyboardButton.hidden != hidden) {
                       self.keyboardButton.hidden = hidden;
                     }

                     if (hidden) {
                       self.keyboardButton.alpha = 0.0;
                     } else {
                       self.keyboardButton.alpha = 1.0;
                     }
                   }];
}

- (void)keyboardButtonPressed {
  base::RecordAction(base::UserMetricsAction("ManualFallback_Close"));
  [self animateKeyboardButtonHidden:YES];
  [self resetTintColors];
  [self.delegate keyboardButtonPressed];
}

- (void)passwordButtonPressed:(UIButton*)sender {
  base::RecordAction(base::UserMetricsAction("ManualFallback_OpenPassword"));
  [self animateKeyboardButtonHidden:NO];
  [self resetTintColors];
  [self.passwordButton setTintColor:UIColor.cr_manualFillTintColor];
  [self.delegate passwordButtonPressed:sender];
}

- (void)cardButtonPressed:(UIButton*)sender {
  base::RecordAction(base::UserMetricsAction("ManualFallback_OpenCreditCard"));
  [self animateKeyboardButtonHidden:NO];
  [self resetTintColors];
  [self.cardsButton setTintColor:UIColor.cr_manualFillTintColor];
  [self.delegate cardButtonPressed:sender];
}

- (void)accountButtonPressed:(UIButton*)sender {
  [self animateKeyboardButtonHidden:NO];
  [self resetTintColors];
  [self.accountButton setTintColor:UIColor.cr_manualFillTintColor];
  [self.delegate accountButtonPressed:sender];
}

@end
