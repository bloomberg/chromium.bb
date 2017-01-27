// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_APP_APP_DELEGATE_H_
#define REMOTING_CLIENT_IOS_APP_APP_DELEGATE_H_

#import <UIKit/UIKit.h>

// Default created delegate class for the entire application.
@interface AppDelegate : UIResponder<UIApplicationDelegate>

@property(strong, nonatomic) UIWindow* window;

@end

#endif  // REMOTING_CLIENT_IOS_APP_APP_DELEGATE_H_

