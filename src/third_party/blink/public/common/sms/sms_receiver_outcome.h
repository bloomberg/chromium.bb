// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SMS_SMS_RECEIVER_OUTCOME_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SMS_SMS_RECEIVER_OUTCOME_H_

namespace blink {

// This enum describes the outcome of the call made to the SMSReceiver API.
enum class SMSReceiverOutcome {
  // Don't change the meaning of these values because they are being recorded
  // in a metric.
  kSuccess = 0,
  kTimeout = 1,
  kConnectionError = 2,
  kCancelled = 3,
  kAborted = 4,
  kMaxValue = kAborted
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SMS_SMS_RECEIVER_OUTCOME_H_
