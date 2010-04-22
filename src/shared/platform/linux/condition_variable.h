/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_LINUX_CONDITION_VARIABLE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_LINUX_CONDITION_VARIABLE_H_

#include <pthread.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/linux/lock.h"
#include "native_client/src/shared/platform/time.h"

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
  NACL_DISALLOW_COPY_AND_ASSIGN(ConditionVariable);
};

}  // namespace NaCl

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_LINUX_CONDITION_VARIABLE_H_
