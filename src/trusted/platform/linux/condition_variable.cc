/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This file provides Linux implementation for the ConditionVariable class

#include "native_client/src/trusted/platform/linux/lock.h"
#include "native_client/src/trusted/platform/linux/condition_variable.h"

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
