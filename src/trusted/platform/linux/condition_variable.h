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


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_LINUX_CONDITION_VARIABLE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_LINUX_CONDITION_VARIABLE_H_

#include <pthread.h>
#include "native_client/src/trusted/platform/linux/lock.h"
#include "native_client/src/trusted/platform/time.h"

#define INFINITE            0xFFFFFFFF  // Infinite timeout

namespace NaCl {

class Lock;

class ConditionVariable {
 public:
  ConditionVariable();

  ~ConditionVariable();

  // We provide two variants of TimedWait:
  // the first one takes relative time as an argument.
  int TimedWaitRel(Lock& user_lock, TimeDelta max_time);
  // The second TimedWait takes absolute time
  int TimedWaitAbs(Lock& user_lock, TimeTicks abs_time);
  void Wait(Lock& user_lock) {
    // Default to "wait forever" timing, which means have to get a Signal()
    // or Broadcast() to come out of this wait state.
    TimedWaitRel(user_lock, TimeDelta::FromMilliseconds(INFINITE));
  }


  // Broadcast() revives all waiting threads.
  void Broadcast();
  // Signal() revives one waiting thread.
  void Signal();

 private:
  pthread_cond_t  cv_;
  DISALLOW_EVIL_CONSTRUCTORS(ConditionVariable);
};

}  // namespace NaCl

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_LINUX_CONDITION_VARIABLE_H_
