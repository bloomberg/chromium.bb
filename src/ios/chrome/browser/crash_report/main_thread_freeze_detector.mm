// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/main_thread_freeze_detector.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ios/chrome/browser/crash_report/crash_report_flags.h"
#import "third_party/breakpad/breakpad/src/client/ios/Breakpad.h"
#import "third_party/breakpad/breakpad/src/client/ios/BreakpadController.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif
namespace {
const char kNsUserDefaultKeyLastSession[] =
    "MainThreadDetectionLastThreadWasFrozen";
const char kNsUserDefaultKeyDelay[] = "MainThreadDetectionDelay";

void LogRecoveryTime(base::TimeDelta time) {
  UMA_HISTOGRAM_TIMES("IOS.MainThreadFreezeDetection.RecoveredAfter", time);
}

}

@interface MainThreadFreezeDetector ()
// The callback that is called regularly on main thread.
- (void)runInMainLoop;
// The callback that is called regularly on watchdog thread.
- (void)runInFreezeDetectionQueue;
// These 4 properties will be accessed from both thread. Make them atomic.
// The date at which |runInMainLoop| was last called.
@property(atomic) NSDate* lastSeenMainThread;
// Whether the watchdog should continue running.
@property(atomic) BOOL running;
// Whether a report was sent.
@property(atomic) BOOL reportSent;
// The delay in seconds after which main thread will be considered frozen.
@property(atomic) NSInteger delay;
@end

@implementation MainThreadFreezeDetector {
  dispatch_queue_t _freezeDetectionQueue;
  BOOL _enabled;
  BOOL _lastSessionEndedFrozen;
  BOOL _experimentChecked;
}

+ (instancetype)sharedInstance {
  static MainThreadFreezeDetector* instance;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    instance = [[MainThreadFreezeDetector alloc] init];
  });
  return instance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _lastSessionEndedFrozen = [[NSUserDefaults standardUserDefaults]
        boolForKey:@(kNsUserDefaultKeyLastSession)];
    [[NSUserDefaults standardUserDefaults]
        setBool:NO
         forKey:@(kNsUserDefaultKeyLastSession)];
    self.delay = [[NSUserDefaults standardUserDefaults]
        integerForKey:@(kNsUserDefaultKeyDelay)];
    _freezeDetectionQueue = dispatch_queue_create(
        "org.chromium.freeze_detection", DISPATCH_QUEUE_SERIAL);
    // Like breakpad, the feature is created immediately in the enabled state as
    // the settings are not available yet when it is started.
    _enabled = YES;
  }
  return self;
}

- (void)setEnabled:(BOOL)enabled {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    int newDelay = crash_report::TimeoutForMainThreadFreezeDetection();
    self.delay = newDelay;
    [[NSUserDefaults standardUserDefaults]
        setInteger:newDelay
            forKey:@(kNsUserDefaultKeyDelay)];
    if (_lastSessionEndedFrozen) {
      LogRecoveryTime(base::TimeDelta::FromSeconds(0));
    }
  });
  _enabled = enabled;
  if (_enabled) {
    [self start];
  } else {
    [self stop];
    [[NSUserDefaults standardUserDefaults]
        setBool:NO
         forKey:@(kNsUserDefaultKeyLastSession)];
  }
}

- (void)start {
  if (self.delay == 0 || self.running || !_enabled) {
    return;
  }
  self.running = YES;
  [self runInMainLoop];
  dispatch_async(_freezeDetectionQueue, ^{
    [self runInFreezeDetectionQueue];
  });
}

- (void)stop {
  self.running = NO;
}

- (void)runInMainLoop {
  if (self.reportSent) {
    self.reportSent = NO;
    [[NSUserDefaults standardUserDefaults]
        setBool:NO
         forKey:@(kNsUserDefaultKeyLastSession)];
    LogRecoveryTime(base::TimeDelta::FromSecondsD(
        [[NSDate date] timeIntervalSinceDate:self.lastSeenMainThread]));
  }
  if (!self.running) {
    return;
  }
  self.lastSeenMainThread = [NSDate date];
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        [self runInMainLoop];
      });
}

- (void)runInFreezeDetectionQueue {
  if (!self.running) {
    return;
  }
  if ([[NSDate date] timeIntervalSinceDate:self.lastSeenMainThread] >
      self.delay) {
    [[BreakpadController sharedInstance]
        withBreakpadRef:^(BreakpadRef breakpadRef) {
          if (!breakpadRef) {
            return;
          }
          BreakpadGenerateReport(breakpadRef, nil);
        }];
    self.reportSent = YES;
    self.running = NO;
    [[NSUserDefaults standardUserDefaults]
        setBool:YES
         forKey:@(kNsUserDefaultKeyLastSession)];
    return;
  }
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)),
                 _freezeDetectionQueue, ^{
                   [self runInFreezeDetectionQueue];
                 });
}

@end
