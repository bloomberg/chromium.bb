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


// NOTE(gregoryd): changed the Windows implementation to use mutex instead
// of CRITICAL_SECTION

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_LINUX_LOCK_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_LINUX_LOCK_H_

#include <pthread.h>
#include "native_client/src/include/base/basictypes.h"
#include "native_client/src/shared/platform/nacl_sync.h"

// This class implements the underlying platform-specific spin-lock mechanism
// used for the Lock class.  Most users should not use LockImpl directly, but
// should instead use Lock.

namespace NaCl {

class Lock {
 friend class ConditionVariable;
 public:
  Lock();
  ~Lock();
  void Acquire();
  void Release();
  bool Try();

 private:
  pthread_mutex_t mutex_;

  DISALLOW_EVIL_CONSTRUCTORS(Lock);
};

// A helper class that acquires the given Lock while the AutoLock is in scope.
class AutoLock {
 public:
  AutoLock(Lock& lock) : lock_(lock) {
    lock_.Acquire();
  }

  ~AutoLock() {
    lock_.Release();
  }

 private:
  Lock& lock_;
  DISALLOW_EVIL_CONSTRUCTORS(AutoLock);
};

// A helper macro to perform a single operation (expressed by expr)
// in a lock
#define LOCKED_EXPRESSION(lock, expr) \
  do { \
    NaCl::AutoLock _auto_lock(lock);  \
    (expr); \
  } while (0)

}  // namespace NaCl

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_LINUX_LOCK_H_
