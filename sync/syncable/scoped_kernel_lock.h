// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_SCOPED_KERNEL_LOCK_H_
#define SYNC_SYNCABLE_SCOPED_KERNEL_LOCK_H_

#include "base/basictypes.h"
#include "base/synchronization/lock.h"

namespace syncer {
namespace syncable {

class Directory;

class ScopedKernelLock {
 public:
  explicit ScopedKernelLock(const Directory* dir);
  ~ScopedKernelLock();

  base::AutoLock scoped_lock_;
  Directory* const dir_;
  DISALLOW_COPY_AND_ASSIGN(ScopedKernelLock);
};

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_SCOPED_KERNEL_LOCK_H_
