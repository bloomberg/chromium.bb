// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MUTEX_H__
#define MUTEX_H__

#include "sandbox_impl.h"

namespace playground {

class Mutex {
 public:
  typedef int mutex_t;

  enum { kInitValue = 0 };

  static void initMutex(mutex_t* mutex) {
    // Mutex is unlocked, and nobody is waiting for it
    *mutex = kInitValue;
  }

  static void unlockMutex(mutex_t* mutex) {
    char status;
    #if defined(__x86_64__) || defined(__i386__)
    asm volatile(
        "lock; addl %2, %0\n"
        "setz %1"
        : "=m"(*mutex), "=qm"(status)
        : "ir"(0x80000000), "m"(*mutex));
    #else
    #error Unsupported target platform
    #endif
    if (status) {
      // Mutex is zero now. No other waiters. So, we can return.
      return;
    }
    // We unlocked the mutex, but still need to wake up other waiters.
    Sandbox::SysCalls sys;
    sys.futex(mutex, FUTEX_WAKE, 1, NULL);
  }

  static bool lockMutex(mutex_t* mutex, int timeout = 0) {
    bool rc        = true;
    // Increment mutex to add ourselves to the list of waiters
    #if defined(__x86_64__) || defined(__i386__)
    asm volatile(
        "lock; incl %0\n"
        : "=m"(*mutex)
        : "m"(*mutex));
    #else
    #error Unsupported target platform
    #endif
    for (;;) {
      // Atomically check whether the mutex is available and if so, acquire it
      char status;
      #if defined(__x86_64__) || defined(__i386__)
      asm volatile(
          "lock; btsl %3, %1\n"
          "setc %0"
          : "=q"(status), "=m"(*mutex)
          : "m"(*mutex), "ir"(31));
      #else
      #error Unsupported target platform
      #endif
      if (!status) {
     done:
        // If the mutex was available, remove ourselves from list of waiters
        #if defined(__x86_64__) || defined(__i386__)
        asm volatile(
            "lock; decl %0\n"
            : "=m"(*mutex)
            : "m"(*mutex));
        #else
        #error Unsupported target platform
        #endif
        return rc;
      }
      int value    = *mutex;
      if (value >= 0) {
        // Mutex has just become available, no need to call kernel
        continue;
      }
      Sandbox::SysCalls sys;
      Sandbox::SysCalls::kernel_timespec tm;
      if (timeout) {
        tm.tv_sec  = timeout / 1000;
        tm.tv_nsec = (timeout % 1000) * 1000 * 1000;
      } else {
        tm.tv_sec  = 0;
        tm.tv_nsec = 0;
      }
      if (NOINTR_SYS(sys.futex(mutex, FUTEX_WAIT, value, &tm)) &&
          sys.my_errno == ETIMEDOUT) {
        rc         = false;
        goto done;
      }
    }
  }

  static bool waitForUnlock(mutex_t* mutex, int timeout = 0) {
    bool rc        = true;
    // Increment mutex to add ourselves to the list of waiters
    #if defined(__x86_64__) || defined(__i386__)
    asm volatile(
        "lock; incl %0\n"
        : "=m"(*mutex)
        : "m"(*mutex));
    #else
    #error Unsupported target platform
    #endif
    Sandbox::SysCalls sys;
    for (;;) {
      mutex_t value = *mutex;
      if (value >= 0) {
     done:
        // Mutex was not locked. Remove ourselves from list of waiters, notify
        // any other waiters (if any), and return.
        #if defined(__x86_64__) || defined(__i386__)
        asm volatile(
            "lock; decl %0\n"
            : "=m"(*mutex)
            : "m"(*mutex));
        #else
        #error Unsupported target platform
        #endif
        NOINTR_SYS(sys.futex(mutex, FUTEX_WAKE, 1, 0));
        return rc;
      }

      // Wait for mutex to become unlocked
      Sandbox::SysCalls::kernel_timespec tm;
      if (timeout) {
        tm.tv_sec   = timeout / 1000;
        tm.tv_nsec  = (timeout % 1000) * 1000 * 1000;
      } else {
        tm.tv_sec   = 0;
        tm.tv_nsec  = 0;
      }

      if (NOINTR_SYS(sys.futex(mutex, FUTEX_WAIT, value, &tm)) &&
          sys.my_errno == ETIMEDOUT) {
        rc          = false;
        goto done;
      }
    }
  }

};

} // namespace

#endif // MUTEX_H__
