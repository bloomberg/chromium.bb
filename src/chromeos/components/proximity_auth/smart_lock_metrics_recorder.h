// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_SMART_LOCK_METRICS_RECORDER_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_SMART_LOCK_METRICS_RECORDER_H_

class SmartLockMetricsRecorder {
 public:
  SmartLockMetricsRecorder();
  ~SmartLockMetricsRecorder();

  enum class SmartLockGetRemoteStatusResultFailureReason {
    kTimedOutBluetoothDisabled = 0,
    kTimedOutCouldNotEstablishAuthenticatedChannel = 1,
    kTimedOutDidNotReceiveRemoteStatusUpdate = 2,
    kUserEnteredPasswordWhileBluetoothDisabled = 3,
    kUserEnteredPasswordWhileConnecting = 4,
    kAuthenticatedChannelDropped = 5,
    kMaxValue = kAuthenticatedChannelDropped
  };

  enum class SmartLockAuthResultFailureReason {
    kUnlockNotAllowed = 0,
    kAlreadyAttemptingAuth = 1,
    kEmptyUserAccount = 2,
    kInvalidAccoundId = 3,
    kAuthAttemptCannotStart = 4,
    kNoPendingOrActiveHost = 5,
    kFailedtoNotifyHostDeviceThatSmartLockWasUsed = 6,
    kFailedToSendUnlockRequest = 7,
    kAuthenticatedChannelDropped = 8,
    kAuthAttemptTimedOut = 9,
    kUnlockEventSentButNotAttemptingAuth = 10,
    kUnlockRequestSentButNotAttemptingAuth = 11,
    kFailedToDecryptSignInChallenge = 12,
    kMaxValue = kFailedToDecryptSignInChallenge
  };

  static void RecordAuthResultUnlockSuccess(bool success = true);
  static void RecordAuthResultUnlockFailure(
      SmartLockAuthResultFailureReason failure_reason);

  static void RecordGetRemoteStatusResultUnlockSuccess(bool success = true);
  static void RecordGetRemoteStatusResultUnlockFailure(
      SmartLockGetRemoteStatusResultFailureReason failure_reason);

  static void RecordAuthResultSignInSuccess(bool success = true);
  static void RecordAuthResultSignInFailure(
      SmartLockAuthResultFailureReason failure_reason);
};

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_SMART_LOCK_METRICS_RECORDER_H_
