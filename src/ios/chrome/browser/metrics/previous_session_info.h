// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_PREVIOUS_SESSION_INFO_H_
#define IOS_CHROME_BROWSER_METRICS_PREVIOUS_SESSION_INFO_H_

#import <Foundation/Foundation.h>

namespace previous_session_info_constants {
// Key in the UserDefaults for a boolean value keeping track of memory warnings.
extern NSString* const kDidSeeMemoryWarningShortlyBeforeTerminating;

// The values of this enum are persisted representing the state of the last
// session (which may have been running a different version of the application).
// Therefore, entries should not be renumbered and numeric values should never
// be reused.
enum class DeviceThermalState {
  kUnknown = 0,
  kNominal = 1,
  kFair = 2,
  kSerious = 3,
  kCritical = 4,
};
}  // namespace previous_session_info_constants

// PreviousSessionInfo has two jobs:
// - Holding information about the last session, persisted across restart.
//   These informations are accessible via the properties on the shared
//   instance.
// - Persist information about the current session, for use in a next session.
@interface PreviousSessionInfo : NSObject

// The thermal state of the device at the end of the previous session.
@property(nonatomic, assign, readonly)
    previous_session_info_constants::DeviceThermalState deviceThermalState;

// Whether the device was in low power mode at the end of the previous session.
@property(nonatomic, assign, readonly) BOOL deviceWasInLowPowerMode;

// Whether the app received a memory warning seconds before being terminated.
@property(nonatomic, assign, readonly)
    BOOL didSeeMemoryWarningShortlyBeforeTerminating;

// Whether or not the system OS was updated between the previous and the
// current session.
@property(nonatomic, assign, readonly) BOOL isFirstSessionAfterOSUpgrade;

// Whether the app was updated between the previous and the current session.
@property(nonatomic, assign, readonly) BOOL isFirstSessionAfterUpgrade;

// Whether the language has been changed between the previous and the current
// session.
@property(nonatomic, assign, readonly) BOOL isFirstSessionAfterLanguageChange;

// Singleton PreviousSessionInfo. During the lifetime of the app, the returned
// object is the same, and describes the previous session, even after a new
// session has started (by calling beginRecordingCurrentSession).
+ (instancetype)sharedInstance;

// Clears the persisted information about the previous session and starts
// persisting information about the current session, for use in a next session.
- (void)beginRecordingCurrentSession;

// Updates the saved last known low power mode setting of the device.
- (void)updateStoredLowPowerMode;

// Updates the saved last known thermal state of the device.
- (void)updateStoredThermalState;

// When a session has begun, records that a memory warning was received.
- (void)setMemoryWarningFlag;

// When a session has begun, records that any memory warning flagged can be
// ignored.
- (void)resetMemoryWarningFlag;

@end

#endif  // IOS_CHROME_BROWSER_METRICS_PREVIOUS_SESSION_INFO_H_
