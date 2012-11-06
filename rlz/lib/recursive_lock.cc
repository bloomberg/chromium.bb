// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/lib/recursive_lock.h"

#include "base/logging.h"

namespace rlz_lib {

RecursiveLock::RecursiveLock()
    : owner_(),
      recursion_() {
}

RecursiveLock::~RecursiveLock() {
}

void RecursiveLock::Acquire() {
  base::subtle::Atomic32 me = base::PlatformThread::CurrentId();
  if (me != base::subtle::NoBarrier_Load(&owner_)) {
    lock_.Acquire();
    DCHECK(!recursion_);
    DCHECK(!owner_);
    base::subtle::NoBarrier_Store(&owner_, me);
  }
  ++recursion_;
}

void RecursiveLock::Release() {
  DCHECK_EQ(base::subtle::NoBarrier_Load(&owner_),
            base::PlatformThread::CurrentId());
  DCHECK_GT(recursion_, 0);
  if (!--recursion_) {
    base::subtle::NoBarrier_Store(&owner_, 0);
    lock_.Release();
  }
}

}  // namespace rlz_lib
