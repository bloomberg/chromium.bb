// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/find_bar/find_bar_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FindBarViewController ()

@property(nonatomic, assign) BOOL darkAppearance;

@end

@implementation FindBarViewController

- (instancetype)initWithDarkAppearance:(BOOL)darkAppearance {
  if (self = [super initWithNibName:nil bundle:nil]) {
    _darkAppearance = darkAppearance;
  }
  return self;
}

#pragma mark - UIView

- (void)loadView {
  self.view = [[FindBarView alloc] initWithDarkAppearance:self.darkAppearance];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
}

#pragma mark - Property Getters

- (FindBarView*)findBarView {
  return base::mac::ObjCCastStrict<FindBarView>(self.view);
}

@end
