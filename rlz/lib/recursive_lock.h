// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_CHROMEOS_LIB_RECURSIVE_LOCK_H_
#define RLZ_CHROMEOS_LIB_RECURSIVE_LOCK_H_

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/synchronization/lock.h"

namespace rlz_lib {


class RecursiveLock {
 public:
  RecursiveLock();
  ~RecursiveLock();

  void Acquire();
  void Release();

 private:
  // Underlying non-recursive lock.
  base::Lock lock_;
  // Owner thread ID.
  base::subtle::Atomic32 owner_;
  // Recursion lock depth.
  int recursion_;
};

}  // namespace rlz_lib

#endif  // RLZ_CHROMEOS_LIB_RECURSIVE_LOCK_H_
