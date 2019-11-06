// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_APP_LAUNCH_MANAGER_H_
#define IOS_TESTING_EARL_GREY_APP_LAUNCH_MANAGER_H_

#import <Foundation/Foundation.h>

// Provides control of the single application-under-test to EarlGrey 2 tests.
@interface AppLaunchManager : NSObject

// Returns the singleton instance of this class.
+ (AppLaunchManager*)sharedManager;

- (instancetype)init NS_UNAVAILABLE;

// Makes sure the app has been started with the appropriate |arguments|.
// In EG2, will launch the app if any of the following conditions are met:
// * The app is not running
// * The app is currently running with different arguments.
// * |forceRestart| is YES
// Otherwise, the app will be activated instead of (re)launched.
// Will wait until app is activated or launched, and fail the test if it
// fails to do so.
// In EG1, this method is a no-op.
- (void)ensureAppLaunchedWithArgs:(NSArray<NSString*>*)arguments
                     forceRestart:(BOOL)forceRestart;

@end

#endif  // IOS_TESTING_EARL_GREY_APP_LAUNCH_MANAGER_H_