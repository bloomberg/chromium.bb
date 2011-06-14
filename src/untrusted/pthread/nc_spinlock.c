/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sched.h>

#include "native_client/src/untrusted/pthread/pthread_types.h"

/*
 * How many times to spin on a lock before calling sched_yield.
 * Keeping this a power of two minus one makes sure that the
 * compiler can reduce the modulus to an and.
 */
#define SPINS_PER_YIELD       63

void nc_spinlock_lock(volatile int *lock) {
  int spins = 0;
  while (__sync_lock_test_and_set(lock, 1)) {
    if ((++spins % SPINS_PER_YIELD) == 0)
      sched_yield();
  }
}

void nc_spinlock_unlock(volatile int *lock) {
  __sync_lock_release(lock);
}
