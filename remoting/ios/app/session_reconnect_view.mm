// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/session_reconnect_view.h"

#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "remoting/ios/app/remoting_theme.h"

#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"

static const CGFloat kPadding = 20.f;

@interface SessionReconnectView () {
  MDCFloatingButton* _reconnectButton;
  UILabel* _errorLabel;
}
@end

@implementation SessionReconnectView

@synthesize delegate = _delegate;

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor clearColor];

    _reconnectButton =
        [MDCFloatingButton floatingButtonWithShape:MDCFloatingButtonShapeMini];
    [_reconnectButton
        setImage:[RemotingTheme
                         .refreshIcon imageFlippedForRightToLeftLayoutDirection]
        forState:UIControlStateNormal];
    [_reconnectButton setBackgroundColor:RemotingTheme.buttonBackgroundColor
                                forState:UIControlStateNormal];

    [_reconnectButton setElevation:4.0f forState:UIControlStateNormal];
    [_reconnectButton setTitle:l10n_util::GetNSString(IDS_RECONNECT)
                      forState:UIControlStateNormal];
    [_reconnectButton addTarget:self
                         action:@selector(didTapReconnect:)
               forControlEvents:UIControlEventTouchUpInside];
    _reconnectButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_reconnectButton];

    _errorLabel = [[UILabel alloc] init];
    _errorLabel.textColor = RemotingTheme.hostErrorColor;
    _errorLabel.font = [UIFont systemFontOfSize:13.f];
    _errorLabel.numberOfLines = 0;
    _errorLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _errorLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_errorLabel];

    [self initializeLayoutConstraintsWithViews:NSDictionaryOfVariableBindings(
                                                   _reconnectButton,
                                                   _errorLabel)];
  }
  return self;
}

- (void)initializeLayoutConstraintsWithViews:(NSDictionary*)views {
  //  NSMutableArray* layoutConstraints = [NSMutableArray array];
  // Metrics to use in visual format strings.
  NSDictionary* layoutMetrics = @{
    @"padding" : @(kPadding),
  };

  [self
      addConstraints:[NSLayoutConstraint
                         constraintsWithVisualFormat:
                             @"H:|-[_errorLabel]-(padding)-[_reconnectButton]-|"
                                             options:0
                                             metrics:layoutMetrics
                                               views:views]];
  [self addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:@"V:|-[_errorLabel]"
                                               options:0
                                               metrics:layoutMetrics
                                                 views:views]];
  [self addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:@"V:|-[_reconnectButton]"
                                               options:0
                                               metrics:layoutMetrics
                                                 views:views]];

  [self setNeedsUpdateConstraints];
}

#pragma mark - Properties

- (void)setErrorText:(NSString*)errorText {
  if (errorText) {
    _errorLabel.text = errorText;
    _errorLabel.hidden = NO;
  } else {
    _errorLabel.hidden = YES;
  }
}

- (NSString*)errorText {
  return _errorLabel.text;
}

#pragma mark - Private

- (void)didTapReconnect:(id)sender {
  if ([_delegate respondsToSelector:@selector(didTapReconnect)]) {
    [_delegate didTapReconnect];
  }
}

@end
