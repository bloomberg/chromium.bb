// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/remoting_theme.h"

@implementation RemotingTheme

+ (UIColor*)hostListBackgroundColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.11f green:0.23f blue:0.66f alpha:1.f];
  });
  return color;
}

+ (UIColor*)connectionViewBackgroundColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.06f green:0.12f blue:0.33f alpha:1.f];
  });
  return color;
}

+ (UIColor*)onlineHostColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.20f green:0.70f blue:0.20f alpha:1.f];
  });
  return color;
}

+ (UIColor*)offlineHostColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.75f green:0.75f blue:0.75f alpha:1.f];
  });
  return color;
}

@end
