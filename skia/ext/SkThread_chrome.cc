// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/skia/include/core/SkThread.h"

#include <new>

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/lock.h"
#include "base/logging.h"

int32_t sk_atomic_inc(int32_t* addr) {
  // sk_atomic_inc is expected to return the old value, Barrier_AtomicIncrement
  // returns the new value.
  return base::subtle::Barrier_AtomicIncrement(addr, 1) - 1;
}

int32_t sk_atomic_dec(int32_t* addr) {
  // sk_atomic_inc is expected to return the old value, Barrier_AtomicIncrement
  // returns the new value.
  return base::subtle::Barrier_AtomicIncrement(addr, -1) + 1;
}

SkMutex::SkMutex(bool isGlobal) : fIsGlobal(isGlobal) {
  COMPILE_ASSERT(sizeof(Lock) <= sizeof(fStorage), Lock_is_too_big_for_SkMutex);
  Lock* lock = reinterpret_cast<Lock*>(fStorage);
  new(lock) Lock();
}

SkMutex::~SkMutex() {
  Lock* lock = reinterpret_cast<Lock*>(fStorage);
  lock->~Lock();
}

void SkMutex::acquire() {
  Lock* lock = reinterpret_cast<Lock*>(fStorage);
  lock->Acquire();
}

void SkMutex::release() {
  Lock* lock = reinterpret_cast<Lock*>(fStorage);
  lock->Release();
}
