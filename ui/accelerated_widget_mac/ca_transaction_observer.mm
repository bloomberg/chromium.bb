// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/ca_transaction_observer.h"

#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"

#import <AppKit/AppKit.h>
#import <CoreFoundation/CoreFoundation.h>
#import <QuartzCore/QuartzCore.h>

typedef enum {
  kCATransactionPhasePreLayout,
  kCATransactionPhasePreCommit,
  kCATransactionPhasePostCommit,
} CATransactionPhase;

API_AVAILABLE(macos(10.11))
@interface CATransaction ()
+ (void)addCommitHandler:(void (^)(void))block
                forPhase:(CATransactionPhase)phase;
@end

namespace ui {

namespace {
NSString* kRunLoopMode = @"Chrome CATransactionCoordinator commit handler";
constexpr auto kPostCommitTimeout = base::TimeDelta::FromMilliseconds(50);
}  // namespace

CATransactionCoordinator& CATransactionCoordinator::Get() {
  static base::NoDestructor<CATransactionCoordinator> instance;
  return *instance;
}

void CATransactionCoordinator::SynchronizeImpl() {
  static bool registeredRunLoopMode = false;
  if (!registeredRunLoopMode) {
    CFRunLoopAddCommonMode(CFRunLoopGetCurrent(),
                           static_cast<CFStringRef>(kRunLoopMode));
    registeredRunLoopMode = true;
  }
  if (active_)
    return;
  active_ = true;

  for (auto& observer : post_commit_observers_)
    observer.OnActivateForTransaction();

  [CATransaction addCommitHandler:^{
    TRACE_EVENT0("ui", "CATransactionCoordinator: pre-commit handler");

    NSDate* start_date = [NSDate date];
    for (;;) {
      base::TimeDelta timeout;
      for (auto& observer : pre_commit_observers_) {
        if (observer.ShouldWaitInPreCommit())
          timeout = std::max(timeout, observer.PreCommitTimeout());
      }
      NSDate* deadline =
          [start_date dateByAddingTimeInterval:timeout.InSecondsF()];
      if ([deadline isLessThanOrEqualTo:[NSDate date]])
        break;
      [NSRunLoop.currentRunLoop runMode:kRunLoopMode beforeDate:deadline];
    }
  }
                         forPhase:kCATransactionPhasePreCommit];

  [CATransaction addCommitHandler:^{
    TRACE_EVENT0("ui", "CATransactionCoordinator: post-commit handler");

    for (auto& observer : post_commit_observers_)
      observer.OnEnterPostCommit();

    NSDate* deadline =
        [NSDate dateWithTimeIntervalSinceNow:kPostCommitTimeout.InSecondsF()];
    for (;;) {
      if (!std::any_of(
              post_commit_observers_.begin(), post_commit_observers_.end(),
              std::mem_fn(&PostCommitObserver::ShouldWaitInPostCommit)))
        break;
      if ([deadline isLessThanOrEqualTo:[NSDate date]])
        break;
      [NSRunLoop.currentRunLoop runMode:kRunLoopMode beforeDate:deadline];
    }
    active_ = false;
  }
                         forPhase:kCATransactionPhasePostCommit];
}

CATransactionCoordinator::CATransactionCoordinator() = default;
CATransactionCoordinator::~CATransactionCoordinator() = default;

void CATransactionCoordinator::Synchronize() {
  if (@available(macos 10.11, *))
    SynchronizeImpl();
}

void CATransactionCoordinator::AddPreCommitObserver(
    PreCommitObserver* observer) {
  pre_commit_observers_.AddObserver(observer);
}

void CATransactionCoordinator::RemovePreCommitObserver(
    PreCommitObserver* observer) {
  pre_commit_observers_.RemoveObserver(observer);
}

void CATransactionCoordinator::AddPostCommitObserver(
    PostCommitObserver* observer) {
  post_commit_observers_.AddObserver(observer);
}

void CATransactionCoordinator::RemovePostCommitObserver(
    PostCommitObserver* observer) {
  post_commit_observers_.RemoveObserver(observer);
}

}  // namespace ui
