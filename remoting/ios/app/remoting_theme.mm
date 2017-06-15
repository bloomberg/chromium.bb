// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/remoting_theme.h"

@implementation RemotingTheme

#pragma mark - Colors

+ (UIColor*)connectionViewBackgroundColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.06f green:0.12f blue:0.33f alpha:1.f];
  });
  return color;
}

+ (UIColor*)hostListBackgroundColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.11f green:0.23f blue:0.66f alpha:1.f];
  });
  return color;
}

+ (UIColor*)menuBlueColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:52.f / 255.f
                            green:174.f / 255.f
                             blue:249.f / 255.f
                            alpha:1.f];
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

+ (UIColor*)onlineHostColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.20f green:0.70f blue:0.20f alpha:1.f];
  });
  return color;
}

#pragma mark - Icons

+ (UIImage*)arrowIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_arrow_forward_white"];
  });
  return icon;
}

+ (UIImage*)backIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"Back"];
  });
  return icon;
}

+ (UIImage*)checkboxCheckedIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_check_box"];
  });
  return icon;
}

+ (UIImage*)checkboxOutlineIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_check_box_outline_blank"];
  });
  return icon;
}

+ (UIImage*)closeIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_close"];
  });
  return icon;
}

+ (UIImage*)desktopIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_desktop"];
  });
  return icon;
}

+ (UIImage*)menuIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_menu"];
  });
  return icon;
}

+ (UIImage*)radioCheckedIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_radio_button_checked"];
  });
  return icon;
}

+ (UIImage*)radioOutlineIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_radio_button_unchecked"];
  });
  return icon;
}

+ (UIImage*)refreshIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_refresh"];
  });
  return icon;
}

+ (UIImage*)settingsIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"Settings"];
  });
  return icon;
}

@end
