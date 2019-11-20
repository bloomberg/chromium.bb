// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/scoped_wake_lock_mac.h"

#include <CoreFoundation/CoreFoundation.h>

#include "platform/api/task_runner.h"
#include "util/logging.h"

namespace openscreen {
namespace platform {

ScopedWakeLockMac::LockState ScopedWakeLockMac::lock_state_{};

SerialDeletePtr<ScopedWakeLock> ScopedWakeLock::Create(
    TaskRunner* task_runner) {
  return SerialDeletePtr<ScopedWakeLock>(task_runner,
                                         new ScopedWakeLockMac(task_runner));
}

ScopedWakeLockMac::ScopedWakeLockMac(TaskRunner* task_runner)
    : task_runner_(task_runner) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  if (lock_state_.reference_count++ == 0) {
    AcquireWakeLock();
  }
}

ScopedWakeLockMac::~ScopedWakeLockMac() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  if (--lock_state_.reference_count == 0) {
    ReleaseWakeLock();
  }
}

void ScopedWakeLockMac::AcquireWakeLock() {
  // The new way of doing an IOPM assertion requires constructing a standard
  // Foundation dictionary and adding the expected properties.
  CFMutableDictionaryRef assertion_properties = CFDictionaryCreateMutable(
      0, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  // This property means that we are requesting that the display not dim
  // or go to sleep.
  CFDictionarySetValue(assertion_properties, kIOPMAssertionTypeKey,
                       kIOPMAssertionTypeNoDisplaySleep);
  CFDictionarySetValue(assertion_properties, kIOPMAssertionNameKey,
                       CFSTR("Open Screen ScopedWakeLock"));

  const IOReturn result = IOPMAssertionCreateWithProperties(
      assertion_properties, &lock_state_.assertion_id);
  OSP_DCHECK_EQ(result, kIOReturnSuccess);
}

void ScopedWakeLockMac::ReleaseWakeLock() {
  const IOReturn result = IOPMAssertionRelease(lock_state_.assertion_id);
  OSP_DCHECK_EQ(result, kIOReturnSuccess);
}

}  // namespace platform
}  // namespace openscreen
