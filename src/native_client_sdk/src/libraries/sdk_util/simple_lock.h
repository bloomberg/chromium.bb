// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_SDK_UTIL_SIMPLE_LOCK_H_
#define LIBRARIES_SDK_UTIL_SIMPLE_LOCK_H_

#include "pthread.h"
#include "sdk_util/macros.h"

namespace sdk_util {

/*
 * SimpleLock
 *
 * A pthread mutex object, with automatic initialization and destruction.
 * Should be used with AutoLock where possible.
 */
class SimpleLock {
 public:
  SimpleLock() {
    pthread_mutex_init(&lock_, NULL);
  }

  ~SimpleLock() {
    pthread_mutex_destroy(&lock_);
  }

  void Lock() const   { pthread_mutex_lock(&lock_); }
  void Unlock() const { pthread_mutex_unlock(&lock_); }

  pthread_mutex_t* mutex() const { return &lock_; }

 private:
  mutable pthread_mutex_t lock_;

  DISALLOW_COPY_AND_ASSIGN(SimpleLock);
};

}  // namespace sdk_util

#endif  // LIBRARIES_SDK_UTIL_SIMPLE_LOCK_H_
