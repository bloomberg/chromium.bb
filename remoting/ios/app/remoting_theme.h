// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_REMOTING_THEME_H_
#define REMOTING_IOS_APP_REMOTING_THEME_H_

#import <UIKit/UIKit.h>

// Styles to be used when rendering the iOS client's UI.
@interface RemotingTheme : NSObject

// Colors

@property(class, nonatomic, readonly) UIColor* connectionViewBackgroundColor;
@property(class, nonatomic, readonly) UIColor* hostListBackgroundColor;
@property(class, nonatomic, readonly) UIColor* menuBlueColor;
@property(class, nonatomic, readonly) UIColor* offlineHostColor;
@property(class, nonatomic, readonly) UIColor* onlineHostColor;

// Icons

@property(class, nonatomic, readonly) UIImage* arrowIcon;
@property(class, nonatomic, readonly) UIImage* backIcon;
@property(class, nonatomic, readonly) UIImage* checkboxCheckedIcon;
@property(class, nonatomic, readonly) UIImage* checkboxOutlineIcon;
@property(class, nonatomic, readonly) UIImage* closeIcon;
@property(class, nonatomic, readonly) UIImage* desktopIcon;
@property(class, nonatomic, readonly) UIImage* menuIcon;
@property(class, nonatomic, readonly) UIImage* radioCheckedIcon;
@property(class, nonatomic, readonly) UIImage* radioOutlineIcon;
@property(class, nonatomic, readonly) UIImage* refreshIcon;
@property(class, nonatomic, readonly) UIImage* settingsIcon;

@end

#endif  // REMOTING_IOS_APP_REMOTING_THEME_H_
