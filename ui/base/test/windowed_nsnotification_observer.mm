// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/windowed_nsnotification_observer.h"

#import <Cocoa/Cocoa.h>

#include "base/run_loop.h"

@interface WindowedNSNotificationObserver ()
- (void)onNotification:(NSNotification*)notification;
@end

@implementation WindowedNSNotificationObserver

- (id)initForNotification:(NSString*)name {
  if ((self = [super init])) {
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(onNotification:)
                                                 name:name
                                               object:nil];
  }
  return self;
}

- (id)initForWorkspaceNotification:(NSString*)name
                          bundleId:(NSString*)bundleId {
  if ((self = [super init])) {
    bundleId_.reset([bundleId copy]);
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserver:self
           selector:@selector(onNotification:)
               name:name
             object:nil];
  }
  return self;
}

- (void)onNotification:(NSNotification*)notification {
  if (bundleId_) {
    NSRunningApplication* application =
        [[notification userInfo] objectForKey:NSWorkspaceApplicationKey];
    if (![[application bundleIdentifier] isEqualToString:bundleId_])
      return;

    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
  } else {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
  }

  notificationReceived_ = YES;
  if (runLoop_)
    runLoop_->Quit();
}

- (void)wait {
  if (notificationReceived_)
    return;

  base::RunLoop runLoop;
  runLoop_ = &runLoop;
  runLoop.Run();
  runLoop_ = nullptr;
}

@end
