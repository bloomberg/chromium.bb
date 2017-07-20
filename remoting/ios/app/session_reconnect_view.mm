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

static const CGFloat kReconnectButtonWidth = 120.f;
static const CGFloat kReconnectButtonHeight = 30.f;

@interface SessionReconnectView () {
  MDCRaisedButton* _reconnectButton;
}
@end

@implementation SessionReconnectView

@synthesize delegate = _delegate;

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor clearColor];

    _reconnectButton = [[MDCRaisedButton alloc] init];
    [_reconnectButton setElevation:4.0f forState:UIControlStateNormal];
    [_reconnectButton setTitle:l10n_util::GetNSString(IDS_RECONNECT)
                      forState:UIControlStateNormal];
    [_reconnectButton addTarget:self
                         action:@selector(didTapReconnect:)
               forControlEvents:UIControlEventTouchUpInside];
    _reconnectButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_reconnectButton];

    [self initializeLayoutConstraintsWithViews:NSDictionaryOfVariableBindings(
                                                   _reconnectButton)];
  }
  return self;
}

- (void)initializeLayoutConstraintsWithViews:(NSDictionary*)views {
  NSMutableArray* layoutConstraints = [NSMutableArray array];

  [layoutConstraints addObject:[_reconnectButton.centerYAnchor
                                   constraintEqualToAnchor:self.centerYAnchor]];

  [layoutConstraints addObject:[_reconnectButton.centerXAnchor
                                   constraintEqualToAnchor:self.centerXAnchor]];

  [layoutConstraints
      addObject:[_reconnectButton.widthAnchor
                    constraintEqualToConstant:kReconnectButtonWidth]];

  [layoutConstraints
      addObject:[_reconnectButton.heightAnchor
                    constraintEqualToConstant:kReconnectButtonHeight]];

  [NSLayoutConstraint activateConstraints:layoutConstraints];
  [self setNeedsUpdateConstraints];
}

#pragma mark - Private

- (void)didTapReconnect:(id)sender {
  if ([_delegate respondsToSelector:@selector(didTapReconnect)]) {
    [_delegate didTapReconnect];
  }
}

@end
