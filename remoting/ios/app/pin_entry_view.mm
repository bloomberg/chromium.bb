// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/pin_entry_view.h"

#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "remoting/ios/app/remoting_theme.h"

#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"

static const CGFloat kMargin = 5.f;
static const CGFloat kPadding = 8.f;
static const CGFloat kLineSpace = 12.f;

static const int kMinPinLength = 6;

@interface PinEntryView ()<UITextFieldDelegate> {
  UISwitch* _pairingSwitch;
  UILabel* _pairingLabel;
  MDCFloatingButton* _pinButton;
  UITextField* _pinEntry;
}
@end

@implementation PinEntryView

@synthesize delegate = _delegate;
@synthesize supportsPairing = _supportsPairing;

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor clearColor];

    _pairingSwitch = [[UISwitch alloc] init];
    _pairingSwitch.tintColor =
        [UIColor colorWithRed:1.f green:1.f blue:1.f alpha:0.5];
    _pairingSwitch.transform = CGAffineTransformMakeScale(0.5, 0.5);
    _pairingSwitch.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_pairingSwitch];

    _pairingLabel = [[UILabel alloc] init];
    _pairingLabel.textColor =
        [UIColor colorWithRed:1.f green:1.f blue:1.f alpha:0.5];
    _pairingLabel.font = [UIFont systemFontOfSize:12.f];
    _pairingLabel.text =
        l10n_util::GetNSString(IDS_REMEMBER_PIN_ON_THIS_DEVICE);
    _pairingLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_pairingLabel];

    _pinButton =
        [MDCFloatingButton floatingButtonWithShape:MDCFloatingButtonShapeMini];
    [_pinButton
        setImage:[RemotingTheme
                         .arrowIcon imageFlippedForRightToLeftLayoutDirection]
        forState:UIControlStateNormal];
    [_pinButton addTarget:self
                   action:@selector(didTapPinEntry:)
         forControlEvents:UIControlEventTouchUpInside];
    _pinButton.translatesAutoresizingMaskIntoConstraints = NO;
    _pinButton.enabled = NO;
    _pinButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_pinButton];

    _pinEntry = [[UITextField alloc] init];
    _pinEntry.textColor = [UIColor whiteColor];
    _pinEntry.secureTextEntry = YES;
    _pinEntry.keyboardType = UIKeyboardTypeNumberPad;
    _pinEntry.attributedPlaceholder = [[NSAttributedString alloc]
        initWithString:l10n_util::GetNSString(IDS_ENTER_PIN)
            attributes:@{
              NSForegroundColorAttributeName :
                  [UIColor colorWithRed:1.f green:1.f blue:1.f alpha:0.5]
            }];
    _pinEntry.translatesAutoresizingMaskIntoConstraints = NO;
    _pinEntry.delegate = self;
    [self addSubview:_pinEntry];

    [self
        initializeLayoutConstraintsWithViews:NSDictionaryOfVariableBindings(
                                                 _pairingSwitch, _pairingLabel,
                                                 _pinButton, _pinEntry)];

    _supportsPairing = YES;
  }
  return self;
}

- (void)initializeLayoutConstraintsWithViews:(NSDictionary*)views {
  // Metrics to use in visual format strings.
  NSDictionary* layoutMetrics = @{
    @"margin" : @(kMargin),
    @"padding" : @(kPadding),
    @"lineSpace" : @(kLineSpace),
  };

  [self addConstraints:
            [NSLayoutConstraint
                constraintsWithVisualFormat:
                    @"H:|-[_pinEntry]-(padding)-[_pinButton]-|"
                                    options:NSLayoutFormatAlignAllCenterY
                                    metrics:layoutMetrics
                                      views:views]];

  [self addConstraints:
            [NSLayoutConstraint
                constraintsWithVisualFormat:
                    @"H:|-[_pairingSwitch]-(padding)-[_pairingLabel]-|"
                                    options:NSLayoutFormatAlignAllCenterY
                                    metrics:layoutMetrics
                                      views:views]];

  [self addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:
                               @"V:|-[_pinButton]-(lineSpace)-[_pairingSwitch]"
                                               options:0
                                               metrics:layoutMetrics
                                                 views:views]];
  [self setNeedsUpdateConstraints];
}

#pragma mark - UIView

- (BOOL)canBecomeFirstResponder {
  return [_pinEntry canBecomeFirstResponder];
}

- (BOOL)becomeFirstResponder {
  return [_pinEntry becomeFirstResponder];
}

- (BOOL)endEditing:(BOOL)force {
  return [_pinEntry endEditing:force];
}

#pragma mark - Properties

- (void)setSupportsPairing:(BOOL)supportsPairing {
  _supportsPairing = supportsPairing;
  _pairingSwitch.hidden = !_supportsPairing;
  [_pairingSwitch setOn:NO animated:NO];
  _pairingLabel.hidden = !_supportsPairing;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)string {
  if (textField == _pinEntry) {
    NSUInteger length = _pinEntry.text.length - range.length + string.length;
    _pinButton.enabled = length >= kMinPinLength;
  }
  return YES;
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  NSLog(@"textFieldShouldReturn");
  if ([_pinButton isEnabled]) {
    [self didTapPinEntry:textField];
    return YES;
  }
  return NO;
}

#pragma mark - Public

- (void)clearPinEntry {
  _pinEntry.text = @"";
  _pinButton.enabled = NO;
}

#pragma mark - Private

- (void)didTapPinEntry:(id)sender {
  [_delegate didProvidePin:_pinEntry.text createPairing:_pairingSwitch.isOn];
  [_pinEntry endEditing:YES];
}

@end
