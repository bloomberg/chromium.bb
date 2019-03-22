// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/smart_lock_metrics_recorder.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"

SmartLockMetricsRecorder::SmartLockMetricsRecorder() = default;

SmartLockMetricsRecorder::~SmartLockMetricsRecorder() {}

void SmartLockMetricsRecorder::RecordAuthResultUnlockSuccess(bool success) {
  UMA_HISTOGRAM_BOOLEAN("SmartLock.AuthResult.Unlock", success);
}

void SmartLockMetricsRecorder::RecordAuthResultUnlockFailure(
    SmartLockAuthResultFailureReason failure_reason) {
  RecordAuthResultUnlockSuccess(false);
  UMA_HISTOGRAM_ENUMERATION("SmartLock.AuthResult.Unlock.Failure",
                            failure_reason);
}

void SmartLockMetricsRecorder::RecordGetRemoteStatusResultUnlockSuccess(
    bool success) {
  UMA_HISTOGRAM_BOOLEAN("SmartLock.GetRemoteStatus.Unlock", success);
}

void SmartLockMetricsRecorder::RecordGetRemoteStatusResultUnlockFailure(
    SmartLockGetRemoteStatusResultFailureReason failure_reason) {
  RecordGetRemoteStatusResultUnlockSuccess(false);
  UMA_HISTOGRAM_ENUMERATION("SmartLock.GetRemoteStatus.Unlock.Failure",
                            failure_reason);
}
