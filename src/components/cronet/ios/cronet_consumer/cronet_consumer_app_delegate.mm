// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "cronet_consumer_app_delegate.h"

#import <Cronet/Cronet.h>

#include "base/format_macros.h"
#import "cronet_consumer_view_controller.h"

@implementation CronetConsumerAppDelegate {
  NSUInteger _counter;
}

@synthesize window;
@synthesize viewController;

// Returns a file name to save net internals logging. This method suffixes
// the ivar |_counter| to the file name so a new name can be obtained by
// modifying that.
- (NSString*)currentNetLogFileName {
  return [NSString
      stringWithFormat:@"cronet-consumer-net-log%" PRIuNS ".json", _counter];
}

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  [Cronet setUserAgent:@"Dummy/1.0" partial:YES];
  [Cronet setQuicEnabled:YES];
  [Cronet start];
  [Cronet startNetLogToFile:[self currentNetLogFileName] logBytes:NO];

  NSURLSessionConfiguration* config =
      [NSURLSessionConfiguration ephemeralSessionConfiguration];
  [Cronet installIntoSessionConfiguration:config];

  // Just for fun, don't route chromium.org requests through Cronet.
  //
  // |chromiumPrefix| is declared outside the scope of the request block so that
  // the block references something outside of its own scope, and cannot be
  // declared as a global block. This makes sure the block is
  // an __NSStackBlock__, and verifies the fix for http://crbug.com/436175 .
  NSString* chromiumPrefix = @"www.chromium.org";
  [Cronet setRequestFilterBlock:^BOOL(NSURLRequest* request) {
    BOOL isChromiumSite = [[[request URL] host] hasPrefix:chromiumPrefix];
    return !isChromiumSite;
  }];

  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  self.viewController =
      [[CronetConsumerViewController alloc] initWithNibName:nil bundle:nil];
  self.window.rootViewController = self.viewController;
  [self.window makeKeyAndVisible];

  return YES;
}

- (void)applicationDidEnterBackground:(UIApplication*)application {
  [Cronet stopNetLog];
}

- (void)applicationWillEnterForeground:(UIApplication*)application {
  _counter++;
  [Cronet startNetLogToFile:[self currentNetLogFileName] logBytes:NO];
}

@end
