/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// This file provides Linux implementation for the ConditionVariable class

#ifdef __native_client__
#include "native_client/src/trusted/service_runtime/include/machine/_types.h"
#endif  // __native_client__
#include "native_client/src/shared/platform/linux/condition_variable.h"
#include "native_client/src/shared/platform/linux/lock.h"

NaCl::ConditionVariable::ConditionVariable() {
  (void) pthread_cond_init(&cv_, (pthread_condattr_t *) 0);
}

NaCl::ConditionVariable::~ConditionVariable() {
  // TODO(gregoryd) - add error handling
  pthread_cond_destroy(&cv_);
}

// Wait() atomically releases the caller's lock as it starts to Wait, and then
// re-acquires it when it is signaled.
int NaCl::ConditionVariable::TimedWaitRel(Lock& user_lock, TimeDelta max_time) {
  TimeTicks ticks = TimeTicks::Now();
  ticks += max_time;
  return TimedWaitAbs(user_lock, ticks);
}

int NaCl::ConditionVariable::TimedWaitAbs(Lock& user_lock, TimeTicks abs_time) {
  struct timespec ts;
  abs_time.InitTimespec(&ts);
  // return 1 for success
  return (0 == pthread_cond_timedwait(&cv_, &user_lock.mutex_, &ts));
}

void NaCl::ConditionVariable::Broadcast() {
  pthread_cond_broadcast(&cv_);
}

void NaCl::ConditionVariable::Signal() {
  pthread_cond_signal(&cv_);
}
