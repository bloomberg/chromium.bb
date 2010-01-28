/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/shared/platform/linux/lock.h"

namespace NaCl {

Lock::Lock() {
  pthread_mutex_init(&mutex_, (pthread_mutexattr_t *) 0);
}

Lock::~Lock() {
  // TODO(gregoryd) - add error handling
  pthread_mutex_destroy(&mutex_);
}

bool Lock::Try() {
  return 0 == pthread_mutex_trylock(&mutex_);
}

void Lock::Acquire() {
  pthread_mutex_lock(&mutex_);
}

void Lock::Release() {
  pthread_mutex_unlock(&mutex_);
}

}  // namespace NaCl
