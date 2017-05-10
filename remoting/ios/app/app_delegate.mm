// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/app_delegate.h"

#include "base/logging.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#import "remoting/ios/app/remoting_view_controller.h"
#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"

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
  [self launchRemotingViewController];
  return YES;
}

- (BOOL)application:(UIApplication*)application handleOpenURL:(NSURL*)url {
  NSMutableDictionary* components = [[NSMutableDictionary alloc] init];
  NSArray* urlComponents = [[url query] componentsSeparatedByString:@"&"];

  for (NSString* componentPair in urlComponents) {
    NSArray* pair = [componentPair componentsSeparatedByString:@"="];
    NSString* key = [[pair firstObject] stringByRemovingPercentEncoding];
    NSString* value = [[pair lastObject] stringByRemovingPercentEncoding];
    [components setObject:value forKey:key];
  }
  NSString* authorizationCode = [components objectForKey:@"code"];

  [[RemotingService SharedInstance].authentication
      authenticateWithAuthorizationCode:authorizationCode];

  [self launchRemotingViewController];
  return YES;
}

- (void)launchRemotingViewController {
  RemotingViewController* vc = [[RemotingViewController alloc] init];
  self.window.rootViewController = vc;
  [self.window makeKeyAndVisible];
}

@end
