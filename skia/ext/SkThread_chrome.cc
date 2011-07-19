// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/skia/include/core/SkThread.h"

#include <new>

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"

int32_t sk_atomic_inc(int32_t* addr) {
  // sk_atomic_inc is expected to return the old value, Barrier_AtomicIncrement
  // returns the new value.
  return base::subtle::Barrier_AtomicIncrement(addr, 1) - 1;
}

int32_t sk_atomic_dec(int32_t* addr) {
  // sk_atomic_dec is expected to return the old value, Barrier_AtomicIncrement
  // returns the new value.
  return base::subtle::Barrier_AtomicIncrement(addr, -1) + 1;
}

SkMutex::SkMutex(bool isGlobal) : fIsGlobal(isGlobal) {
  COMPILE_ASSERT(sizeof(base::Lock) <= sizeof(fStorage), Lock_is_too_big_for_SkMutex);
  base::Lock* lock = reinterpret_cast<base::Lock*>(fStorage);
  new(lock) base::Lock();
}

SkMutex::~SkMutex() {
  base::Lock* lock = reinterpret_cast<base::Lock*>(fStorage);
  lock->~Lock();
}

void SkMutex::acquire() {
  base::Lock* lock = reinterpret_cast<base::Lock*>(fStorage);
  lock->Acquire();
}

void SkMutex::release() {
  base::Lock* lock = reinterpret_cast<base::Lock*>(fStorage);
  lock->Release();
}
