// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_REMOTING_THEME_H_
#define REMOTING_IOS_APP_REMOTING_THEME_H_

#import <UIKit/UIKit.h>

// Styles to be used when rendering the iOS client's UI.
@interface RemotingTheme : NSObject

@property(class, nonatomic, readonly) UIColor* hostListBackgroundColor;
@property(class, nonatomic, readonly) UIColor* connectionViewBackgroundColor;
@property(class, nonatomic, readonly) UIColor* onlineHostColor;
@property(class, nonatomic, readonly) UIColor* offlineHostColor;

@end

#endif  // REMOTING_IOS_APP_REMOTING_THEME_H_
