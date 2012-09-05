/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// This module defines the interface for using platform specific mutexes.
// It is expected that the Allocation function will return NULL on any
// failure instead of throwing an exception.  This module is expected
// to throw a std::exception when an unexpected OS error is encoutered.
//
// The mutex is owned by a single thread at a time.  The thread that
// owns the mutex is free to call Lock multiple times, however it must
// call Unlock the same number of times to release the lock.
#ifndef NATIVE_CLIENT_PORT_MUTEX_H_
#define NATIVE_CLIENT_PORT_MUTEX_H_ 1

#include <assert.h>
#include <stddef.h>

#include "native_client/src/shared/platform/nacl_sync_checked.h"

namespace port {

// TODO(mseaborn): This is no longer an interface class so it could
// be renamed from "IMutex" to "Mutex".  Or we could simply remove it
// and inline the methods in the source.
class IMutex {
 public:
  inline IMutex() {
    NaClXMutexCtor(&mutex_);
  }

  inline void Lock() {
    NaClXMutexLock(&mutex_);
  }

  inline void Unlock() {
    NaClXMutexUnlock(&mutex_);
  }

  static inline IMutex *Allocate() {
    return new IMutex();
  }

  static inline void Free(IMutex *mtx) {
    delete mtx;
  }

 private:
  inline ~IMutex() {
    NaClMutexDtor(&mutex_);
  }

  struct NaClMutex mutex_;
};


// MutexLock
//   A MutexLock object will lock on construction and automatically
// unlock on destruction of the object as the object goes out of scope.
class MutexLock {
 public:
  explicit MutexLock(IMutex *mutex) : mutex_(mutex) {
    assert(NULL != mutex_);
    mutex_->Lock();
  }
  ~MutexLock() {
    mutex_->Unlock();
  }

 private:
  IMutex *mutex_;
  MutexLock(const MutexLock&);
  MutexLock &operator=(const MutexLock&);
};


}  // namespace port

#endif  // NATIVE_CLIENT_PORT_MUTEX_H_

