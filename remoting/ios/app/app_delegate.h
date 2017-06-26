// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_APP_DELEGATE_H_
#define REMOTING_IOS_APP_APP_DELEGATE_H_

#import <UIKit/UIKit.h>

// Default created delegate class for the entire application.
@interface AppDelegate : UIResponder<UIApplicationDelegate>

- (void)showMenuAnimated:(BOOL)animated;
- (void)hideMenuAnimated:(BOOL)animated;
- (void)presentSignInFlow;

@property(strong, nonatomic) UIWindow* window;
@property(class, strong, nonatomic, readonly) AppDelegate* instance;

// This will push the FAQ view controller onto the provided nav controller.
- (void)navigateToFAQs:(UINavigationController*)navigationController;

// This will push the Help Center view controller onto the provided nav
// controller.
- (void)navigateToHelpCenter:(UINavigationController*)navigationController;

// This will push the Send Feedback view controller onto the provided nav
// controller.
- (void)navigateToSendFeedback:(UINavigationController*)navigationController;

@end

#endif  // REMOTING_IOS_APP_APP_DELEGATE_H_

