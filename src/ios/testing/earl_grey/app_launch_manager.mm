// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/app_launch_manager.h"

#import <XCTest/XCTest.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)  // avoid unused function warning in EG1
namespace {
// Checks if two pairs of launch arguments are equivalent.
bool LaunchArgumentsAreEqual(NSArray<NSString*>* args1,
                             NSArray<NSString*>* args2) {
  // isEqualToArray will only return true if both arrays are non-nil,
  // so first check if both arrays are empty or nil
  if (!args1.count && !args2.count) {
    return true;
  }

  return [args1 isEqualToArray:args2];
}
}  // namespace
#endif

@interface AppLaunchManager ()
@property(nonatomic) XCUIApplication* runningApplication;
@property(nonatomic) NSArray<NSString*>* currentLaunchArgs;
@end

@implementation AppLaunchManager

+ (AppLaunchManager*)sharedManager {
  static AppLaunchManager* instance = nil;
  static dispatch_once_t guard;
  dispatch_once(&guard, ^{
    instance = [[AppLaunchManager alloc] initPrivate];
  });
  return instance;
}

- (instancetype)initPrivate {
  self = [super init];
  return self;
}

- (void)ensureAppLaunchedWithArgs:(NSArray<NSString*>*)arguments
                     forceRestart:(BOOL)forceRestart {
#if defined(CHROME_EARL_GREY_2)
  bool appNeedsLaunching =
      forceRestart || !self.runningApplication ||
      !LaunchArgumentsAreEqual(arguments, self.currentLaunchArgs);

  if (!appNeedsLaunching) {
    [self.runningApplication activate];
    return;
  }

  XCUIApplication* application = [[XCUIApplication alloc] init];
  application.launchArguments = arguments;

  [application launch];
  self.runningApplication = application;
  self.currentLaunchArgs = arguments;
#endif
}
@end
