// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/providers/chromium_logo_controller.h"

#import <UIKit/UIKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ChromiumLogoController

@synthesize showingLogo = _showingLogo;
@synthesize view = _view;

- (instancetype)init {
  if ((self = [super init])) {
    _view = [[UIView alloc] init];
  }
  return self;
}

- (void)fetchDoodle {
  // Do nothing.
}

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return nil;
}

@end
