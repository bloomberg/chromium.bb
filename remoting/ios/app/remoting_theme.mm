// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/remoting_theme.h"

@implementation RemotingTheme

#pragma mark - Colors

+ (UIColor*)firstLaunchViewBackgroundColor {
  return UIColor.whiteColor;
}

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

+ (UIColor*)hostListRefreshIndicatorColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.50f green:0.87f blue:0.92f alpha:1.f];
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

+ (UIColor*)hostOfflineColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithWhite:0.87f alpha:1.f];
  });
  return color;
}

+ (UIColor*)hostOnlineColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.40f green:0.75f blue:0.40f alpha:1.f];
  });
  return color;
}

+ (UIColor*)hostErrorColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:249.f / 255.f
                            green:146.f / 255.f
                             blue:34.f / 255.f
                            alpha:1.f];
  });
  return color;
}

+ (UIColor*)buttonBackgroundColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.11f green:0.55f blue:0.95f alpha:1.f];
  });
  return color;
}

+ (UIColor*)buttonTextColor {
  return UIColor.whiteColor;
}

+ (UIColor*)flatButtonTextColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithRed:0.11f green:0.55f blue:0.95f alpha:1.f];
  });
  return color;
}

+ (UIColor*)setupListBackgroundColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithWhite:1.f alpha:0.9f];
  });
  return color;
}

+ (UIColor*)setupListTextColor {
  return UIColor.grayColor;
}

+ (UIColor*)setupListNumberColor {
  return UIColor.whiteColor;
}

+ (UIColor*)sideMenuIconColor {
  static UIColor* color;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    color = [UIColor colorWithWhite:0.f alpha:0.54f];
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
    icon = [UIImage imageNamed:@"ic_chevron_left_white_36pt"];
  });
  return icon;
}

+ (UIImage*)checkboxCheckedIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_check_box_white"];
  });
  return icon;
}

+ (UIImage*)checkboxOutlineIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_check_box_outline_blank_white"];
  });
  return icon;
}

+ (UIImage*)closeIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_close_white"];
  });
  return icon;
}

+ (UIImage*)desktopIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_desktop_windows_white"];
  });
  return icon;
}

+ (UIImage*)menuIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_menu_white"];
  });
  return icon;
}

+ (UIImage*)radioCheckedIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_radio_button_checked_white"];
  });
  return icon;
}

+ (UIImage*)radioOutlineIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_radio_button_unchecked_white"];
  });
  return icon;
}

+ (UIImage*)refreshIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_refresh_white"];
  });
  return icon;
}

+ (UIImage*)settingsIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_settings_white"];
  });
  return icon;
}

+ (UIImage*)helpIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_help"];
  });
  return icon;
}

+ (UIImage*)feedbackIcon {
  static UIImage* icon;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    icon = [UIImage imageNamed:@"ic_feedback"];
  });
  return icon;
}

@end
