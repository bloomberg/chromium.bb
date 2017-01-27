// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/app/app_delegate.h"

#include "base/logging.h"

#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#import "remoting/client/ios/app/example_view_controller.h"


@implementation AppDelegate

@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    willFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  self.window =
      [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  self.window.backgroundColor = [UIColor whiteColor];
  return YES;
}

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {

  ExampleViewController *vc = [[ExampleViewController alloc] init];

  self.window.rootViewController = vc;

  [self.window makeKeyAndVisible];
  return YES;
}

@end
