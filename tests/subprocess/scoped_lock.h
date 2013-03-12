// -*- c++ -*-
// Copyright (c) 2013 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_TESTS_SUBPROCESS_SCOPED_LOCK_H_

#include <pthread.h>

// no name space, since this is so common
class MutexLocker {
 public:
  explicit MutexLocker(pthread_mutex_t *lock)
      : lock_(lock) {
    pthread_mutex_lock(lock_);
  }
  ~MutexLocker() {
    pthread_mutex_unlock(lock_);
  }
 private:
  pthread_mutex_t *lock_;
};

#endif  // NATIVE_CLIENT_TESTS_SUBPROCESS_SCOPED_LOCK_H_
