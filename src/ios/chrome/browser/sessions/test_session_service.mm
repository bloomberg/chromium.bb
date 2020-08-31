// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/test_session_service.h"

#include "base/memory/ref_counted.h"
#include "base/threading/thread_task_runner_handle.h"
#import "ios/chrome/browser/sessions/session_ios_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TestSessionService

- (instancetype)init {
  return [super initWithTaskRunner:base::ThreadTaskRunnerHandle::Get()];
}

- (void)saveSession:(__weak SessionIOSFactory*)factory
          directory:(NSString*)directory
        immediately:(BOOL)immediately {
  NSString* sessionPath = [[self class] sessionPathForDirectory:directory];
  NSData* data =
      [NSKeyedArchiver archivedDataWithRootObject:[factory sessionForSaving]
                            requiringSecureCoding:NO
                                            error:nil];
  if (self.performIO)
    [self performSaveSessionData:data sessionPath:sessionPath];
  _saveSessionCallsCount++;
}

@end
