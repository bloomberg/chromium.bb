// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/pin_entry_view.h"

#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "remoting/ios/app/remoting_theme.h"

static const CGFloat kMargin = 5.f;
static const CGFloat kPadding = 6.f;
static const CGFloat kLineSpace = 12.f;

@interface PinEntryView () {
  UISwitch* _pairingSwitch;
  UILabel* _pairingLabel;
  MDCFloatingButton* _pinButton;
  UITextField* _pinEntry;
}
@end

@implementation PinEntryView

@synthesize delegate = _delegate;

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor clearColor];

    _pairingSwitch = [[UISwitch alloc] init];
    _pairingSwitch.tintColor =
        [UIColor colorWithRed:1.f green:1.f blue:1.f alpha:0.5];
    _pairingSwitch.transform = CGAffineTransformMakeScale(0.5, 0.5);
    [self addSubview:_pairingSwitch];

    _pairingLabel = [[UILabel alloc] init];
    _pairingLabel.textColor =
        [UIColor colorWithRed:1.f green:1.f blue:1.f alpha:0.5];
    _pairingLabel.font = [UIFont systemFontOfSize:12.f];
    _pairingLabel.text = @"Remember my PIN on this device.";
    [self addSubview:_pairingLabel];

    _pinButton =
        [MDCFloatingButton floatingButtonWithShape:MDCFloatingButtonShapeMini];
    [_pinButton setImage:RemotingTheme.arrowIcon forState:UIControlStateNormal];
    [_pinButton addTarget:self
                   action:@selector(didTapPinEntry:)
         forControlEvents:UIControlEventTouchUpInside];
    _pinButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_pinButton];

    _pinEntry = [[UITextField alloc] init];
    _pinEntry.textColor = [UIColor whiteColor];
    _pinEntry.secureTextEntry = YES;
    _pinEntry.keyboardType = UIKeyboardTypeNumberPad;
    // TODO(nicholss): L18N this.
    _pinEntry.attributedPlaceholder = [[NSAttributedString alloc]
        initWithString:@"Enter PIN"
            attributes:@{
              NSForegroundColorAttributeName :
                  [UIColor colorWithRed:1.f green:1.f blue:1.f alpha:0.5]
            }];
    [self addSubview:_pinEntry];
  }
  return self;
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

- (void)layoutSubviews {
  [super layoutSubviews];

  [_pinButton sizeToFit];
  CGFloat buttonSize = _pinButton.frame.size.width;  // Assume circle.

  _pinEntry.frame =
      CGRectMake(kMargin, 0.f,
                 self.frame.size.width - kPadding - kMargin * 2.f - buttonSize,
                 buttonSize);

  [_pinButton sizeToFit];
  _pinButton.frame =
      CGRectMake(self.frame.size.width - kPadding - kMargin - buttonSize, 0.f,
                 buttonSize, buttonSize);

  [_pairingSwitch sizeToFit];
  _pairingSwitch.center = CGPointMake(
      kMargin + _pairingSwitch.frame.size.width / 2.f,
      buttonSize + _pairingSwitch.frame.size.height / 2.f + kLineSpace);

  _pairingLabel.frame =
      CGRectMake(kMargin + _pairingSwitch.frame.size.width + kPadding,
                 buttonSize + kLineSpace, 0.f, 0.f);
  [_pairingLabel sizeToFit];
}

#pragma mark - Private

- (void)didTapPinEntry:(id)sender {
  [_delegate didProvidePin:_pinEntry.text createPairing:_pairingSwitch.isOn];
  [_pinEntry endEditing:YES];
}

@end
