// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_WINDOWED_NSNOTIFICATION_OBSERVER_H_
#define UI_BASE_TEST_WINDOWED_NSNOTIFICATION_OBSERVER_H_

#import <Foundation/Foundation.h>

#import "base/mac/scoped_nsobject.h"

namespace base {
class RunLoop;
}

// Like WindowedNotificationObserver in content/public/test/test_utils.h, this
// starts watching for a notification upon construction and can wait until the
// notification is observed. This guarantees that notifications fired between
// calls to init and wait will be caught.
@interface WindowedNSNotificationObserver : NSObject {
 @private
  base::scoped_nsobject<NSString> bundleId_;
  int notificationCount_;
  base::RunLoop* runLoop_;
}

@property(readonly, nonatomic) int notificationCount;

// Watch for an NSNotification on the default notification center for the given
// |notificationSender|, or a nil object if omitted.
- (id)initForNotification:(NSString*)name;

// Watch for an NSNotification on the default notification center from a
// particular object.
- (id)initForNotification:(NSString*)name object:(id)sender;

// Watch for an NSNotification on the shared workspace notification center for
// the given application.
- (id)initForWorkspaceNotification:(NSString*)name bundleId:(NSString*)bundleId;

// Waits for |minimumCount| notifications to be observed and returns YES.
// Returns NO on a timeout. This can be called multiple times.
- (BOOL)waitForCount:(int)minimumCount;

// Returns YES if any notification has been observed, or spins a RunLoop until
// it either is observed, or a timeout occurs. Returns NO on a timeout.
- (BOOL)wait;
@end

#endif  // UI_BASE_TEST_WINDOWED_NSNOTIFICATION_OBSERVER_H_
